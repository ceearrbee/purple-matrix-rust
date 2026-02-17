use matrix_sdk::encryption::verification::{SasVerification, Verification};
use matrix_sdk::Client;
use std::ffi::CString;
use crate::ffi::{SAS_REQUEST_CALLBACK, SAS_HAVE_EMOJI_CALLBACK, SHOW_VERIFICATION_QR_CALLBACK};

pub async fn handle_verification_request(client: Client, event: matrix_sdk::ruma::events::key::verification::request::ToDeviceKeyVerificationRequestEvent) {
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    let sender = event.sender.as_str();
    let flow_id = event.content.transaction_id.as_str();

    log::info!("Received verification request from {} with flow {}", sender, flow_id);

    if let (Ok(c_user_id), Ok(c_sender), Ok(c_flow)) = (CString::new(user_id), CString::new(sender), CString::new(flow_id)) {
        let guard = SAS_REQUEST_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_flow.as_ptr());
        }
    }
}

pub async fn handle_sas_verification(sas: SasVerification, client: Client) {
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    let target = sas.other_user_id().as_str();
    // Fallback: use other_user_id as context since flow_id() is missing in this version
    let flow_id = sas.other_user_id().as_str();

    log::info!("Handling SAS verification flow for {}", target);

    if let Some(emojis) = sas.emoji() {
        let mut emoji_str = String::new();
        for emoji in emojis {
            emoji_str.push_str(&format!("{} ({}) ", emoji.symbol, emoji.description));
        }

        if let (Ok(c_user_id), Ok(c_target), Ok(c_flow), Ok(c_emojis)) = 
           (CString::new(user_id.clone()), CString::new(target), CString::new(flow_id), CString::new(emoji_str)) 
        {
            let guard = SAS_HAVE_EMOJI_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_user_id.as_ptr(), c_target.as_ptr(), c_flow.as_ptr(), c_emojis.as_ptr());
            }
        }
    }
}

pub async fn handle_verification_change(verification: Verification, client: Client) {
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    match verification {
        Verification::SasV1(sas) => handle_sas_verification(sas, client).await,
        Verification::QrV1(qr) => {
            if let Ok(_data) = qr.to_qr_code() {
                let target = qr.other_user_id().as_str();
                let html = format!("Scan this QR code in your other Matrix client to verify with {}: (QR data present)", target);
                
                if let (Ok(c_user_id), Ok(c_target), Ok(c_html)) = 
                   (CString::new(user_id), CString::new(target), CString::new(html)) 
                {
                    let guard = SHOW_VERIFICATION_QR_CALLBACK.lock().unwrap();
                    if let Some(cb) = *guard {
                        cb(c_user_id.as_ptr(), c_target.as_ptr(), c_html.as_ptr());
                    }
                }
            }
        }
        _ => {}
    }
}
