use matrix_sdk::media::{MediaFormat, MediaRequestParameters};
use matrix_sdk::ruma::events::room::MediaSource;
use matrix_sdk::Client;
use std::path::PathBuf;


pub fn get_media_dir() -> PathBuf {
    let base_path = if let Some(path) = crate::DATA_PATH.lock().unwrap_or_else(|e| e.into_inner()).clone() {
        path
    } else {
        std::env::temp_dir()
    };
    
    let mut path = base_path;
    path.push("media");
    if !path.exists() {
        let _ = std::fs::create_dir_all(&path);
    }
    path
}

pub async fn download_avatar(client: &Client, url: &matrix_sdk::ruma::OwnedMxcUri, identifier: &str) -> Option<String> {
    let safe_id = identifier.replace(":", "_").replace("@", "").replace("!", "");
    let filename = format!("avatar_{}", safe_id);
    let mut path = get_media_dir();
    path.push(filename);
    
    if path.exists() {
        return Some(path.to_string_lossy().to_string());
    }

    let request = MediaRequestParameters {
        source: MediaSource::Plain(url.clone()),
        format: MediaFormat::File,
    };

    match client.media().get_media_content(&request, true).await {
        Ok(bytes) => {
            if let Ok(mut file) = std::fs::File::create(&path) {
                use std::io::Write;
                if file.write_all(&bytes).is_ok() {
                    return Some(path.to_string_lossy().to_string());
                }
            }
            None
        }
        Err(e) => {
            let err_str = e.to_string();
            if err_str.contains("401") || err_str.contains("M_UNKNOWN_TOKEN") {
                log::error!("Authentication failed while downloading avatar: {}", err_str);
            } else {
                log::warn!("Failed to download avatar {}: {:?}", url, e);
            }
            None
        }
    }
}

pub async fn cleanup_media_files() {
    let temp_dir = get_media_dir();
    if let Ok(mut entries) = tokio::fs::read_dir(&temp_dir).await {
        while let Ok(Some(entry)) = entries.next_entry().await {
            let path = entry.path();
            if let Ok(metadata) = tokio::fs::metadata(&path).await {
                if !metadata.is_file() {
                    continue;
                }
            }
            // Cleanup old files or just all?
            // For now, let's just clean those matching our patterns if we want to be safe, 
            // but since we are in a dedicated subdir, we can likely clean everything older than X.
            // Keeping implementation simple: delete if it looks like ours or just everything in our folder.
            if let Some(filename) = path.file_name().and_then(|n| n.to_str()) {
                if filename.starts_with("matrix_") || filename.starts_with("avatar_") {
                    if let Err(e) = tokio::fs::remove_file(&path).await {
                        log::warn!("Failed to delete temp file {:?}: {}", path, e);
                    } else {
                        log::debug!("Deleted temp file {:?}", path);
                    }
                }
            }
        }
    }
}
