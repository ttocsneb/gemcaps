use std::{path::Path, io, ffi::OsStr, os::unix::prelude::OsStrExt};

use tokio::fs;

use crate::gemini::Response;
use crate::{gemini::Request, log::Logger, pathutil, config::file::FileConf};
use crate::error::{GemcapsError, GemcapsResult};

async fn clean_path(root: impl AsRef<Path>, path: impl AsRef<OsStr>, request_path: impl AsRef<str>) -> GemcapsResult<Option<Response>> {
    let root = root.as_ref();
    let mut path = path.as_ref();
    let request_path = request_path.as_ref();
    let mut redirect_path = request_path.to_string();

    let mut file = root.join(&path);

    loop {
        match fs::metadata(&file).await {
            Ok(metadata) => {
                if metadata.is_dir() && !redirect_path.ends_with('/') {
                    redirect_path += "/";
                }
                if redirect_path != request_path {
                    return Ok(Some(Response::redirect_perm(pathutil::encode(&redirect_path))));
                }
                return Ok(None);
            },
            Err(err) => {
                if redirect_path.ends_with('/') {
                    redirect_path = redirect_path[..redirect_path.len() - 1].to_string();
                    path = OsStr::from_bytes(&path.as_bytes()[..path.len() - 1]);
                    file = root.join(&path);
                    continue;
                }
                return Err(err.into());
            },
        }
    }
}

pub async fn process_file_request(request: &Request, conf: &FileConf, logger: &mut Logger) -> GemcapsResult<Option<Response>> {
    let original_path = urlencoding::decode_binary(request.path().as_bytes());


    let path = OsStr::from_bytes(match original_path.starts_with("/".as_bytes()) {
        true => &original_path[1..],
        false => &original_path,
    });

    let original_path = OsStr::from_bytes(&original_path).to_string_lossy();


    // Make sure that the path is safe and does not leave the root directory
    let file = conf.file_root.join(match pathutil::traversal_safe(path) {
        Ok(val) => val,
        Err(err) => {
            logger.error(format!("Invalid path: {}", err)).await;
            return Ok(Some(Response::failure_perm("Permission denied")));
        }
    });

    let original_path = pathutil::expand(&original_path)?;

    match clean_path(&conf.file_root, path, &original_path).await {
        Ok(Some(redirect)) => {
            return Ok(Some(redirect));
        },
        Ok(None) => {},
        Err(GemcapsError::Io(err)) => {
            if err.kind() == io::ErrorKind::NotFound {
                return Ok(None);
            }
            return Err(err.into());
        }
        Err(err) => {
            return Err(err.into());
        }
    }

    let metadata = fs::metadata(&file).await?;

    if metadata.is_file() {
        let mime_type = pathutil::get_mimetype(&file);
        return Ok(Some(Response::success(mime_type, fs::read_to_string(file).await?)));
    }
    
    if metadata.is_dir() {
        let mut dirs = fs::read_dir(file).await?;
        let mut folders = vec![];
        let mut files = vec![];
        while let Some(ent) = dirs.next_entry().await? {
            let name = urlencoding::encode_binary(ent.file_name().as_bytes()).into_owned();
            let ent_meta = fs::metadata(ent.path()).await?;
            if ent_meta.is_dir() {
                folders.push(format!("{}/", name));
                continue;
            }
            for index in &conf.indexes {
                if index.matches(&name) {
                    let file = ent.path();
                    let mime_type = pathutil::get_mimetype(&file);
                    return Ok(Some(Response::success(mime_type, fs::read_to_string(file).await?)));
                }
            }
            files.push(name);
        }
        if !conf.send_folders {
            return Err(io::Error::new(io::ErrorKind::PermissionDenied, "Not allowed to send folders").into());
        }

        let mut response = format!("# {}\n\n", original_path);
        if let Some(parent) = pathutil::parent(&original_path) {
            response += &format!("=> {} ⬅️  Back\n\n", pathutil::encode(&parent));
        }
        folders.sort();
        for folder in folders {
            let url = &pathutil::join(&pathutil::encode(&original_path), &folder);
            let name = String::from_utf8_lossy(&urlencoding::decode_binary(&folder.as_bytes())).into_owned();
            response += &format!("=> {} 📂 {}\n", url, name);
        }
        files.sort();
        for file in files {
            let url = &pathutil::join(&pathutil::encode(&original_path), &file);
            let name = String::from_utf8_lossy(&urlencoding::decode_binary(&file.as_bytes())).into_owned();
            response += &format!("=> {} 📃 {}\n", url, name);
        }

        return Ok(Some(Response::success("text/gemini", response)));
    }

    logger.error(format!("{:?} is neither a file or a folder", file)).await;
    return Ok(Some(Response::failure_perm("Permission denied")));
}
