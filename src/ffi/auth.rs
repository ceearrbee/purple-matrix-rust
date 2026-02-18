use crate::ffi::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init_login_failed_cb(cb: LoginFailedCallback) {
    if let Ok(mut guard) = LOGIN_FAILED_CALLBACK.lock() {
        *guard = Some(cb);
    }
}
