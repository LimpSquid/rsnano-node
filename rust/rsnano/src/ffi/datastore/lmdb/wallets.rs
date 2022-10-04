use std::ffi::{c_char, CStr};

use crate::datastore::lmdb::LmdbWallets;

use super::{
    iterator::{to_lmdb_iterator_handle, LmdbIteratorHandle},
    TransactionHandle,
};

pub struct LmdbWalletsHandle(LmdbWallets);

#[no_mangle]
pub extern "C" fn rsn_lmdb_wallets_create() -> *mut LmdbWalletsHandle {
    Box::into_raw(Box::new(LmdbWalletsHandle(LmdbWallets::new())))
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallets_destroy(handle: *mut LmdbWalletsHandle) {
    drop(Box::from_raw(handle))
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallets_db_handle(handle: *mut LmdbWalletsHandle) -> u32 {
    (*handle).0.handle
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallets_set_db_handle(
    handle: *mut LmdbWalletsHandle,
    db_handle: u32,
) {
    (*handle).0.handle = db_handle;
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallets_init(
    handle: *mut LmdbWalletsHandle,
    txn: &mut TransactionHandle,
) -> bool {
    (*handle).0.initialize((*txn).as_txn()).is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallets_get_store_it(
    handle: *mut LmdbWalletsHandle,
    txn: &mut TransactionHandle,
    hash: *const c_char,
) -> *mut LmdbIteratorHandle {
    let mut it = (*handle)
        .0
        .get_store_it((*txn).as_txn(), CStr::from_ptr(hash).to_str().unwrap());
    to_lmdb_iterator_handle(it.as_mut())
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallets_move_table(
    handle: *mut LmdbWalletsHandle,
    name: *const c_char,
    txn_source: &mut TransactionHandle,
    txn_destination: &mut TransactionHandle,
) {
    (*handle)
        .0
        .move_table(
            CStr::from_ptr(name).to_str().unwrap(),
            (*txn_source).as_txn(),
            (*txn_destination).as_txn(),
        )
        .unwrap();
}
