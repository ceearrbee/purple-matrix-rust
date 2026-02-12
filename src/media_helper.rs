use matrix_sdk::media::{MediaFormat, MediaRequestParameters};
use matrix_sdk::ruma::events::room::MediaSource;
use matrix_sdk::Client;
use std::path::PathBuf;

pub async fn download_avatar(client: &Client, url: &matrix_sdk::ruma::OwnedMxcUri, identifier: &str) -> Option<String> {
    let safe_id = identifier.replace(":", "_").replace("@", "").replace("!", "");
    let filename = format!("avatar_{}", safe_id);
    let path = PathBuf::from(format!("/tmp/matrix_avatars/{}", filename));
    
    if path.exists() {
        return Some(path.to_string_lossy().to_string());
    }

    let _ = std::fs::create_dir_all("/tmp/matrix_avatars");
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
    let temp_dir = std::env::temp_dir();
    if let Ok(mut entries) = tokio::fs::read_dir(&temp_dir).await {
        while let Ok(Some(entry)) = entries.next_entry().await {
            let path = entry.path();
            if let Some(filename) = path.file_name().and_then(|n| n.to_str()) {
                if filename.starts_with("matrix_") {
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
