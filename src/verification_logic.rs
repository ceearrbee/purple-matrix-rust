use matrix_sdk::encryption::verification::{SasVerification, Verification};
use matrix_sdk::Client;


pub async fn handle_verification_request(client: Client, event: matrix_sdk::ruma::events::key::verification::request::ToDeviceKeyVerificationRequestEvent) {
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    let sender = event.sender.as_str();
    let flow_id = event.content.transaction_id.as_str();

    log::info!("Received verification request from {} with flow {}", sender, flow_id);

    let event = crate::ffi::FfiEvent::SasRequest {
        user_id: user_id,
        target_user_id: sender.to_string(),
        flow_id: flow_id.to_string(),
    };
    let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
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

        let event = crate::ffi::FfiEvent::SasHaveEmoji {
            user_id: user_id.clone(),
            target_user_id: target.to_string(),
            flow_id: flow_id.to_string(),
            emojis: emoji_str,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
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
                
                let event = crate::ffi::FfiEvent::ShowVerificationQr {
                    user_id: user_id.clone(),
                    target_user_id: target.to_string(),
                    html_data: html,
                };
                let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
            }
        }
        _ => {}
    }
}
