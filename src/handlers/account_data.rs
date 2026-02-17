use matrix_sdk::ruma::events::AnyGlobalAccountDataEvent;
use matrix_sdk::Client;

pub async fn handle_account_data(event: AnyGlobalAccountDataEvent, _client: Client) {
    if let AnyGlobalAccountDataEvent::IgnoredUserList(ev) = event {
        log::info!("Received updated ignored user list with {} users", ev.content.ignored_users.len());
    }
}
