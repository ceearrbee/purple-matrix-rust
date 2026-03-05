use matrix_sdk::encryption::verification::SasVerification;
use matrix_sdk::Client;
use crate::ffi::{send_event, events::FfiEvent};

pub async fn handle_sas_request(sas: SasVerification, client: Client) {
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    let target_user_id = sas.other_user_id().to_string();
    
    // In matrix-sdk 0.16, SasVerification has other_device() which returns DeviceData.
    let flow_id = sas.other_device().device_id().to_string();

    log::info!("Verification request from {} (flow: {})", target_user_id, flow_id);

    let event = FfiEvent::SasRequest {
        user_id,
        target_user_id,
        flow_id,
    };
    send_event(event);
}

pub async fn handle_sas_emoji(sas: SasVerification, client: Client) {
    if let Some(emojis) = sas.emoji() {
        let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let target_user_id = sas.other_user_id().to_string();
        let flow_id = sas.other_device().device_id().to_string();

        let mut emoji_str = String::new();
        for (i, emoji) in emojis.iter().enumerate() {
            if i > 0 { emoji_str.push_str(", "); }
            emoji_str.push_str(emoji.description);
            emoji_str.push(' ');
            emoji_str.push_str(emoji.symbol);
        }

        let event = FfiEvent::SasHaveEmoji {
            user_id,
            target_user_id,
            flow_id,
            emojis: emoji_str,
        };
        send_event(event);
    }
}

pub async fn handle_qr_verification(client: Client, target_user_id: String, html_data: String) {
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    
    let event = FfiEvent::ShowVerificationQr {
        user_id,
        target_user_id,
        html_data,
    };
    send_event(event);
}
