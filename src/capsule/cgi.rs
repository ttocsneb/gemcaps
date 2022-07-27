use std::{io, ffi::{OsStr, OsString}, os::unix::prelude::OsStrExt, path::PathBuf, env, net::SocketAddr, collections::HashMap};

use lazy_static::lazy_static;
use regex::Regex;
use tokio::{fs::{self, File}, io::{AsyncWriteExt, BufReader, AsyncBufReadExt}, process};

use crate::{config::{cgi::CgiConf, CapsuleConf}, gemini::{Request, Response}, log::Logger, pathutil, error::{GemcapsResult, GemcapsError}};

struct CgiResponse {
    path: String,
    file: PathBuf,
    cgi_path: String,
}

/// Find the cgi file for the given request
/// 
/// This will find the path to the cgi file as well as determine the url path
/// to the cgi file and path to pass to the cgi file.
/// 
/// `gemini://localhost/cgi/foo/bar/cheese\r\n` -> `{file: "example/dist/cgi/index.py", path: "/cgi", cgi_path: "/foo/bar/cheese"}`
async fn find_cgi_file(request: &Request, conf: &CgiConf) -> io::Result<CgiResponse> {
    let original_path = urlencoding::decode_binary(request.path().as_bytes());

    let path = OsStr::from_bytes(match original_path.starts_with("/".as_bytes()) {
        true => &original_path[1..],
        false => &original_path,
    });

    let original_path = pathutil::encode_binary(&original_path);

    // Make sure that the path is safe and does not leave the root directory
    let mut file = conf.cgi_root.join(match pathutil::traversal_safe(path) {
        Ok(val) => val,
        Err(_) => {
            return Err(io::Error::new(io::ErrorKind::PermissionDenied, "Attempted to leave root directory"));
        }
    });

    let original_path = pathutil::expand(&original_path)?;
    let mut cgi_path = match original_path.ends_with("/") {
        true => &original_path[..original_path.len() - 1],
        false => &original_path,
    }.to_owned();


    fn matches_extension(file: &PathBuf, conf: &CgiConf) -> bool {
        if conf.extensions.is_empty() {
            return true;
        }
        if let Some(extension) = file.extension() {
            let extension = String::from_utf8_lossy(extension.as_bytes());
            for ext in &conf.extensions {
                if ext == extension.as_ref() {
                    return true
                }
            }
        }
        false
    }

    loop {
        if let Some(meta) = fs::metadata(&file).await.ok() {
            if meta.is_file() {
                if matches_extension(&file, conf) {
                    return Ok(CgiResponse { 
                        cgi_path: original_path.replacen(&cgi_path, "", 1),
                        path: cgi_path, 
                        file, 
                    });
                }
            }
            // Search for a valid index file in the directory
            if meta.is_dir() {
                let mut children = fs::read_dir(&file).await?;
                while let Some(child) = children.next_entry().await? {
                    let child_path = child.path();
                    let child_name = urlencoding::encode_binary(child_path.file_name().unwrap().as_bytes());
                    let mut valid = false;
                    for index in &conf.indexes {
                        if index.matches(&child_name) {
                            valid = true;
                            break;
                        }
                    }
                    if !valid {
                        continue;
                    }
                    let meta = child.metadata().await?;
                    if meta.is_file() && matches_extension(&child_path, conf) {
                        return Ok(CgiResponse { 
                            cgi_path: original_path.replacen(&cgi_path, "", 1),
                            path: cgi_path,
                            file: child_path,
                        })
                    }
                }
            }
        }

        file = file.parent().ok_or(
            io::Error::new(io::ErrorKind::NotFound, "File does not exist")
        )?.to_owned();
        let parent = pathutil::parent(&cgi_path).ok_or(
            io::Error::new(io::ErrorKind::NotFound, "File does not exist")
        )?;
        cgi_path = if parent.ends_with("/") {
            parent[..parent.len() - 1].to_owned()
        } else {
            parent.into_owned()
        };
    }
}


pub async fn process_cgi_request(request: &Request, config: &CapsuleConf, conf: &CgiConf, addr: &SocketAddr, logger: &mut Logger) -> GemcapsResult<Option<Response>> {
    lazy_static! {
        static ref SHEBANG: Regex = Regex::new(r"\s+").unwrap();
    }
    let (path, file, cgi_path) = match find_cgi_file(request, conf).await {
        Ok(value) => (value.path, value.file, value.cgi_path),
        Err(err) => {
            if err.kind() == io::ErrorKind::NotFound {
                return Ok(None);
            }
            return Err(err.into());
        }
    };
    
    let file = if file.is_absolute() {
        file
    } else {
        env::current_dir()?.join(file)
    };

    // Read the first line of the file to see if there is a shebang
    //
    // This is done here for compatability with windows. It may be a bad idea
    // to support shebangs on windows since this could cause security
    // vulnerabilities on linux/mac since it is no longer required to be
    // executable to run the script if a shebang is provided.
    let mut reader = BufReader::new(File::open(&file).await?);
    let mut line = String::with_capacity(128);
    reader.read_line(&mut line).await?;
    reader.shutdown().await?;

    let mut args = Vec::new();
    let command = if line.starts_with("#!") {
        let mut shebang = SHEBANG.split(line[2..].trim());
        let command = shebang.next().ok_or(
            GemcapsError::new("Invalid shebang")
        )?;
        args = shebang.map(|s| OsString::from(s)).collect();
        args.push(file.as_os_str().to_owned());
        
        OsStr::from_bytes(command.as_bytes())
    } else {
        file.as_os_str()
    };

    let mut optional_envs: HashMap<OsString, OsString> = HashMap::new();

    if !cgi_path.is_empty() {
        let info = OsStr::from_bytes(&urlencoding::decode_binary(cgi_path.as_bytes())).to_owned();
        let translated = conf.cgi_root.join(OsStr::from_bytes(if info.as_bytes().starts_with(b"/") {
            &info.as_bytes()[1..]
        } else {
            info.as_bytes()
        }));
        optional_envs.insert("PATH_INFO".into(), info);
        optional_envs.insert("PATH_TRANSLATED".into(), if translated.is_absolute() {
                translated
            } else {
                env::current_dir()?.join(translated)
            }.into());
    }
    if let Some(path) = env::var_os("PATH") {
        optional_envs.insert("PATH".into(), path);
    }
    if !request.query().is_empty() {
        optional_envs.insert("QUERY_STRING".into(), request.query().into());
    }

    let output = process::Command::new(command)
        .args(args)
        .env_clear()
        .env("GATEWAY_INTERFACE", "1.1")
        .env("REMOTE_ADDR", addr.ip().to_string())
        .env("REMOTE_HOST", addr.ip().to_string())
        .env("REQUEST_METHOD", "")
        .env("SCRIPT_NAME", OsStr::from_bytes(&urlencoding::decode_binary(path.as_bytes())))
        .env("SERVER_NAME", request.domain())
        .env("SERVER_PORT", &config.listen[config.listen.rfind(":").unwrap() + 1..])
        .env("SERVER_PROTOCOL", "GEMINI")
        .env("SERVER_SOFTWARE", format!("{}/{}", env!("CARGO_PKG_NAME"), env!("CARGO_PKG_VERSION")))
        .env("GEMINI_DOCUMENT_ROOT", file.parent().unwrap())
        .env("GEMINI_SCRIPT_FILENAME", &file)
        .env("GEMINI_URL", request.uri())
        .env("GEMINI_URL_PATH", OsStr::from_bytes(&urlencoding::decode_binary(request.path().as_bytes())))
        .envs(optional_envs)
        .kill_on_drop(true)
        .output().await?;

    if !output.stderr.is_empty() {
        logger.error(String::from_utf8_lossy(&output.stderr)).await;
    }
    if !output.status.success() {
        logger.error(format!("CGI script exited with status {}", output.status)).await;
        return Ok(Some(Response::cgi_error(format!("exited with status {}", output.status))));
    }
    Ok(Some(match Response::parse(output.stdout) {
        Ok(val) => val,
        Err(err) => {
            logger.error(err.to_string()).await;
            Response::cgi_error("invalid response header")
        },
    }))
}