use matrix_sdk::encryption::verification::{SasVerification, VerificationRequest};
use matrix_sdk::Client;
use crate::ffi::{send_event, events::FfiEvent};

pub async fn handle_verification_request_with_state(request: VerificationRequest, client: Client) {
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    let target_user_id = request.other_user_id().to_string();
    let flow_id = request.flow_id().to_string();

    log::info!("Verification request from {} (flow: {})", target_user_id, flow_id);

    let event = FfiEvent::SasRequest {
        user_id: user_id.clone(),
        target_user_id: target_user_id.clone(),
        flow_id: flow_id.clone(),
    };
    send_event(event);

    // Monitor for transition to SAS
    use futures_util::StreamExt;
    use matrix_sdk::encryption::verification::VerificationRequestState;
    let mut state_stream = request.changes();
    while let Some(state) = state_stream.next().await {
        if let VerificationRequestState::Transitioned { verification } = state {
            if let Some(sas) = verification.sas() {
                handle_sas_request(sas, client.clone()).await;
                break;
            }
        }
        if request.is_cancelled() || request.is_done() {
            break;
        }
    }
}

pub async fn handle_sas_request(sas: SasVerification, client: Client) {
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    let target_user_id = sas.other_user_id().to_string();
    
    // In matrix-sdk 0.16, SasVerification has other_device() which returns DeviceData.
    let flow_id = sas.other_device().device_id().to_string();

    log::info!("Verification request from {} (flow: {})", target_user_id, flow_id);

    crate::ffi::encryption::ACTIVE_SAS.insert(flow_id.clone(), sas);

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

        crate::ffi::encryption::ACTIVE_SAS.insert(flow_id.clone(), sas);

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
