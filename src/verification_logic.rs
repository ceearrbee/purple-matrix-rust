use matrix_sdk::Client;
use matrix_sdk::ruma::events::key::verification::request::ToDeviceKeyVerificationRequestEvent;
use std::ffi::CString;
use crate::ffi::{SAS_REQUEST_CALLBACK, SAS_HAVE_EMOJI_CALLBACK, SHOW_VERIFICATION_QR_CALLBACK};
use futures_util::StreamExt;
use qrcode::QrCode;
use image::Luma;
use std::io::Cursor;
use base64::{Engine as _, engine::general_purpose};

pub async fn handle_verification_request(client: Client, event: ToDeviceKeyVerificationRequestEvent) {
    let user_id = event.sender.to_string();
    let flow_id = event.content.transaction_id.to_string();
    
    log::info!("Handling verification request from {} (flow: {})", user_id, flow_id);
    
    // Trigger C callback to ask user if they want to verify.
    let c_user = CString::new(user_id.clone()).unwrap_or_default();
    let c_flow = CString::new(flow_id.clone()).unwrap_or_default();
    
    {
        let guard = SAS_REQUEST_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_user.as_ptr(), c_flow.as_ptr());
        }
    }

    // Now monitor the request for state changes
    if let Some(request) = client.encryption().get_verification_request(
        &event.sender,
        &flow_id
    ).await {
        use matrix_sdk::encryption::verification::VerificationRequestState;

        // Monitor changes
        let mut stream = request.changes();
        while let Some(state) = stream.next().await {
            match state {
                VerificationRequestState::Transitioned { verification } => {
                    use matrix_sdk::encryption::verification::Verification;
                    match verification {
                        Verification::SasV1(sas) => {
                            log::info!("Verification transitioned to SAS for flow {}", flow_id);
                            // Monitor SAS changes
                            let mut sas_stream = sas.changes();
                            while let Some(sas_state) = sas_stream.next().await {
                                use matrix_sdk::encryption::verification::SasState;
                                match sas_state {
                                    SasState::KeysExchanged { emojis, .. } => {
                                        if let Some(emojis) = emojis {
                                            let emoji_str = emojis.emojis.iter().map(|e| format!("{} ({})", e.symbol, e.description)).collect::<Vec<_>>().join(", ");
                                            log::info!("SAS Emojis for {}: {}", user_id, emoji_str);
                                            
                                            if let (Ok(c_user), Ok(c_flow), Ok(c_emojis)) = 
                                                (CString::new(user_id.clone()), CString::new(flow_id.clone()), CString::new(emoji_str)) 
                                            {
                                                let guard = SAS_HAVE_EMOJI_CALLBACK.lock().unwrap();
                                                if let Some(cb) = *guard {
                                                    cb(c_user.as_ptr(), c_flow.as_ptr(), c_emojis.as_ptr());
                                                }
                                            }
                                        }
                                    },
                                    SasState::Done { .. } => {
                                        log::info!("SAS Verification Done for {}", user_id);
                                        break;
                                    },
                                    SasState::Cancelled(info) => {
                                        log::warn!("SAS Verification Cancelled for {}: {:?}", user_id, info);
                                        break;
                                    },
                                    _ => {}
                                }
                            }
                            break; // Request transitioned, SAS loop finished
                        },
                        Verification::QrV1(qr) => {
                             log::info!("Verification transitioned to QR for flow {}", flow_id);
                             
                             // Try to get bytes.
                             if let Ok(bytes) = qr.to_bytes() {
                                 // bytes is Vec<u8>
                                 if let Ok(code) = QrCode::new(bytes) {
                                     let image = code.render::<Luma<u8>>().build();
                                     let dyn_image = image::DynamicImage::ImageLuma8(image);
                                     let mut buffer = Vec::new();
                                     let mut cursor = Cursor::new(&mut buffer);
                                     if let Ok(_) = dyn_image.write_to(&mut cursor, image::ImageOutputFormat::Png) {
                                         let b64 = general_purpose::STANDARD.encode(buffer);
                                         let html = format!("<div style=\"text-align:center;\"><img src=\"data:image/png;base64,{}\"><br>Scan this code to verify.</div>", b64);
                                         
                                         let c_user = CString::new(user_id.clone()).unwrap_or_default();
                                         let c_html = CString::new(html).unwrap_or_default();
                                         
                                         let guard = SHOW_VERIFICATION_QR_CALLBACK.lock().unwrap();
                                         if let Some(cb) = *guard {
                                             cb(c_user.as_ptr(), c_html.as_ptr());
                                         }
                                     }
                                 }
                             }
                             break; // Request transitioned
                        },
                        _ => {}
                    }
                },

                VerificationRequestState::Cancelled(info) => {
                    log::warn!("Verification Request Cancelled for {}: {:?}", user_id, info);
                    break;
                },
                VerificationRequestState::Done => {
                    log::info!("Verification Request Done for {}", user_id);
                    break;
                },
                _ => {}
            }
        }
    }
}