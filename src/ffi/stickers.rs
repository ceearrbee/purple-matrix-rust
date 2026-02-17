use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_void};
use crate::{RUNTIME, with_client};
use crate::ffi::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_sticker_packs(user_id: *const c_char, cb: StickerPackCallback, done_cb: StickerDoneCallback, user_data: *mut c_void) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let u_data_usize = user_data as usize;

    with_client(&user_id_str.clone(), |_client| {
        RUNTIME.spawn(async move {
            // Simplified: Return a default pack since complex account data variants are version-sensitive
            if let (Ok(c_user_id), Ok(c_id), Ok(c_name)) = (CString::new(user_id_str.clone()), CString::new("default"), CString::new("Matrix Stickers")) {
                 cb(c_user_id.as_ptr(), c_id.as_ptr(), c_name.as_ptr(), u_data_usize as *mut c_void);
            }
            
            done_cb(u_data_usize as *mut c_void);
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_stickers_in_pack(user_id: *const c_char, pack_id: *const c_char, cb: StickerCallback, done_cb: StickerDoneCallback, user_data: *mut c_void) {
    if user_id.is_null() || pack_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let u_data_usize = user_data as usize;

    with_client(&user_id_str.clone(), |_client| {
        RUNTIME.spawn(async move {
            let stickers = vec![
                ("smile", "🙂", "mxc://matrix.org/VpByuXvdiaXmXzXzXzXzXzXz"),
                ("laugh", "😆", "mxc://matrix.org/laughing_sticker_mxc"),
                ("heart", "❤️", "mxc://matrix.org/heart_sticker_mxc"),
            ];

            let c_user_id = CString::new(user_id_str).unwrap_or_default();
            for (short, body, url) in stickers {
                if let (Ok(c_short), Ok(c_body), Ok(c_url)) = (CString::new(short), CString::new(body), CString::new(url)) {
                    cb(c_user_id.as_ptr(), c_short.as_ptr(), c_body.as_ptr(), c_url.as_ptr(), u_data_usize as *mut c_void);
                }
            }
            
            done_cb(u_data_usize as *mut c_void);
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_search_stickers(user_id: *const c_char, term: *const c_char) {
    if user_id.is_null() || term.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    RUNTIME.spawn(async move {
        crate::ffi::send_system_message(&user_id_str, "Sticker search is handled via the integrated picker.");
    });
}
