use std::path::Path;

use tokio::fs;

use crate::{gemini::Request, log::Logger, error::GemcapsError, config::FileConf, pathutil};

use super::CapsuleResponse;

async fn clean_path(root: impl AsRef<Path>, path: impl Into<String>, request: &Request) -> Result<Option<CapsuleResponse>, GemcapsError> {
    let root = root.as_ref();
    let mut path = path.into();
    let mut redirect_path = request.path().to_string();

    let mut file = root.join(&path);

    loop {
        match fs::metadata(&file).await {
            Ok(metadata) => {
                if metadata.is_dir() && !redirect_path.ends_with('/') {
                    redirect_path += "/";
                }
                if redirect_path != request.path() {
                    if !request.query().is_empty() {
                        redirect_path += "?";
                        redirect_path += request.query();
                        return Ok(Some(CapsuleResponse::redirect_temp(redirect_path)));
                    }
                    return Ok(Some(CapsuleResponse::redirect_perm(redirect_path)));
                }
                return Ok(None);
            },
            Err(err) => {
                if redirect_path.ends_with('/') {
                    redirect_path = redirect_path[..redirect_path.len() - 1].to_string();
                    path = path[..path.len() - 1].to_string();
                    file = root.join(&path);
                    continue;
                }
                return Err(err.into());
            },
        }
    }
}

pub async fn process_file_request(request: &Request, conf: &FileConf, logger: &mut Logger) -> Result<Option<CapsuleResponse>, GemcapsError> {
    // TODO! parse url encoded characters

    let path = match request.path().starts_with('/') {
        true => &request.path()[1..],
        false => &request.path(),
    };

    // Make sure that the path is safe and does not leave the root directory
    let file = conf.file_root.join(match pathutil::traversal_safe(path) {
        Ok(val) => val,
        Err(err) => {
            logger.error(format!("Invalid path: {}", err)).await;
            return Ok(Some(CapsuleResponse::failure_perm("Permission denied")));
        }
    });

    if let Some(redirect) = clean_path(&conf.file_root, path, &request).await? {
        return Ok(Some(redirect));
    }

    let metadata = fs::metadata(&file).await?;

    if metadata.is_file() {
        let mime_type = pathutil::get_mimetype(&file);
        return Ok(Some(CapsuleResponse::success(mime_type, fs::read_to_string(file).await?)));
    }
    
    if metadata.is_dir() {
        let mut dirs = fs::read_dir(file).await?;
        let mut folders = vec![];
        let mut files = vec![];
        while let Some(ent) = dirs.next_entry().await? {
            // TODO: encode name
            let name = match ent.file_name().to_str() {
                Some(val) => val,
                None => {
                    logger.error(format!("{:?} is not unicode and cannot be represented", ent.file_name())).await;
                    continue;
                }
            }.to_string();
            let ent_meta = fs::metadata(ent.path()).await?;
            if ent_meta.is_dir() {
                folders.push(format!("{}/", name));
                continue;
            }
            files.push(name);
            // TODO: deal with index
        }
        // TODO: check if folders can be sent

        let mut response = format!("# {}\n\n", request.path());
        folders.sort();
        for folder in folders {
            response += &format!("=> {} 📂 {}\n", pathutil::join(request.path(), &folder), folder);
        }
        files.sort();
        for file in files {
            response += &format!("=> {} 📃 {}\n", pathutil::join(request.path(), &file), file);
        }

        return Ok(Some(CapsuleResponse::success("text/gemini", response)));
    }

    logger.error(format!("{:?} is neither a file or a folder", file)).await;
    return Ok(Some(CapsuleResponse::failure_perm("Permission denied")));
}
