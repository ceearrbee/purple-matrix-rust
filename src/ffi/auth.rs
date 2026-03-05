use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client, CLIENTS};
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_login_with_password(user_data: *mut std::os::raw::c_void, password: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_data.is_null() || password.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_data as *const c_char).to_string_lossy().into_owned() };
        let _password_str = unsafe { CStr::from_ptr(password).to_string_lossy().into_owned() };

        log::info!("Attempting password login for {}", user_id_str);
        // We'd need homeserver and data_dir here. 
        // For now, let's assume they are handled or we use defaults.
        // This is a complex flow that usually happens in matrix_account.c
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_login_with_sso(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, |client: Client| {
            log::info!("Starting SSO flow for {}", user_id_str);
            crate::auth::start_sso_flow(client);
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_homeserver(user_data: *mut std::os::raw::c_void, homeserver: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_data.is_null() || homeserver.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_data as *const c_char).to_string_lossy().into_owned() };
        let hs_str = unsafe { CStr::from_ptr(homeserver).to_string_lossy().into_owned() };

        log::info!("Setting homeserver for {} to {}", user_id_str, hs_str);
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_clear_session_cache(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        log::info!("Clearing session cache for {}", user_id_str);
        CLIENTS.remove(&user_id_str);
    })
}
