use std::{
    ffi::{c_char, CStr},
    ops::Deref,
    path::PathBuf,
    str::FromStr,
};

use crate::{
    datastore::lmdb::{LmdbWalletStore, WalletValue},
    ffi::{copy_account_bytes, copy_public_key_bytes, copy_raw_key_bytes, wallet::kdf::KdfHandle},
    Account, PublicKey, RawKey,
};

use super::{
    iterator::{to_lmdb_iterator_handle, LmdbIteratorHandle},
    TransactionHandle,
};

pub struct LmdbWalletStoreHandle(LmdbWalletStore);

#[repr(C)]
pub struct WalletValueDto {
    pub key: [u8; 32],
    pub work: u64,
}

impl From<WalletValue> for WalletValueDto {
    fn from(value: WalletValue) -> Self {
        WalletValueDto {
            key: *value.key.as_bytes(),
            work: value.work,
        }
    }
}

impl From<&WalletValueDto> for WalletValue {
    fn from(dto: &WalletValueDto) -> Self {
        WalletValue::new(RawKey::from_bytes(dto.key), dto.work)
    }
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_create(
    fanout: usize,
    kdf: *const KdfHandle,
) -> *mut LmdbWalletStoreHandle {
    Box::into_raw(Box::new(LmdbWalletStoreHandle(LmdbWalletStore::new(
        fanout,
        (*kdf).deref().clone(),
    ))))
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_destroy(handle: *mut LmdbWalletStoreHandle) {
    drop(Box::from_raw(handle))
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_initialize(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    path: *const c_char,
) -> bool {
    let p = PathBuf::from_str(CStr::from_ptr(path).to_str().unwrap()).unwrap();
    (*handle).0.initialize((*txn).as_txn(), &p).is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_db_handle(
    handle: *mut LmdbWalletStoreHandle,
) -> u32 {
    (*handle).0.db_handle()
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_set_db_handle(
    handle: *mut LmdbWalletStoreHandle,
    dbi: u32,
) {
    (*handle).0.set_db_handle(dbi);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_wallet_key_mem(
    handle: *mut LmdbWalletStoreHandle,
    key: *mut u8,
) {
    let k = (*handle).0.fans.lock().unwrap().wallet_key_mem.value();
    copy_raw_key_bytes(k, key);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_set_wallet_key_mem(
    handle: *mut LmdbWalletStoreHandle,
    key: *const u8,
) {
    (*handle)
        .0
        .fans
        .lock()
        .unwrap()
        .wallet_key_mem
        .value_set(RawKey::from_ptr(key));
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_password(
    handle: *mut LmdbWalletStoreHandle,
    password: *mut u8,
) {
    let k = (*handle).0.fans.lock().unwrap().password.value();
    copy_raw_key_bytes(k, password);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_set_password(
    handle: *mut LmdbWalletStoreHandle,
    password: *const u8,
) {
    (*handle)
        .0
        .fans
        .lock()
        .unwrap()
        .password
        .value_set(RawKey::from_ptr(password));
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_entry_put_raw(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    account: *const u8,
    entry: *const WalletValueDto,
) {
    (*handle).0.entry_put_raw(
        (*txn).as_txn(),
        &Account::from_ptr(account),
        &WalletValue::from(&*entry),
    )
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_entry_get_raw(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    account: *const u8,
    result: *mut WalletValueDto,
) {
    let entry = (*handle)
        .0
        .entry_get_raw((*txn).as_txn(), &Account::from_ptr(account));
    *result = entry.into()
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_check(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    result: *mut u8,
) {
    let value = (*handle).0.check((*txn).as_txn());
    copy_raw_key_bytes(value, result);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_salt(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    result: *mut u8,
) {
    let value = (*handle).0.salt((*txn).as_txn());
    copy_raw_key_bytes(value, result);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_wallet_key(
    handle: *mut LmdbWalletStoreHandle,
    prv_key: *mut u8,
    txn: *mut TransactionHandle,
) {
    let key = (*handle).0.wallet_key((*txn).as_txn());
    copy_raw_key_bytes(key, prv_key);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_seed(
    handle: *mut LmdbWalletStoreHandle,
    prv_key: *mut u8,
    txn: *mut TransactionHandle,
) {
    let key = (*handle).0.seed((*txn).as_txn());
    copy_raw_key_bytes(key, prv_key);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_seed_set(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    prv_key: *const u8,
) {
    (*handle)
        .0
        .set_seed((*txn).as_txn(), &RawKey::from_ptr(prv_key));
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_deterministic_index_get(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
) -> u32 {
    (*handle).0.deterministic_index_get((*txn).as_txn())
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_deterministic_index_set(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    index: u32,
) {
    (*handle).0.deterministic_index_set((*txn).as_txn(), index);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_valid_password(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
) -> bool {
    (*handle).0.valid_password((*txn).as_txn())
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_derive_key(
    handle: *mut LmdbWalletStoreHandle,
    prv: *mut u8,
    txn: *mut TransactionHandle,
    password: *const c_char,
) {
    let password = CStr::from_ptr(password).to_str().unwrap();
    let key = (*handle).0.derive_key((*txn).as_txn(), password);
    copy_raw_key_bytes(key, prv);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_rekey(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    password: *const c_char,
) -> bool {
    let password = CStr::from_ptr(password).to_str().unwrap();
    (*handle).0.rekey((*txn).as_txn(), password).is_ok()
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_begin(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
) -> *mut LmdbIteratorHandle {
    let mut iterator = (*handle).0.begin((*txn).as_txn());
    to_lmdb_iterator_handle(iterator.as_mut())
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_begin_at_account(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    account: *const u8,
) -> *mut LmdbIteratorHandle {
    let mut iterator = (*handle)
        .0
        .begin_at_account((*txn).as_txn(), &Account::from_ptr(account));
    to_lmdb_iterator_handle(iterator.as_mut())
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_erase(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    account: *const u8,
) {
    (*handle)
        .0
        .erase((*txn).as_txn(), &Account::from_ptr(account));
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_key_type(value: *const WalletValueDto) -> u8 {
    LmdbWalletStore::key_type(&WalletValue::from(&*value)) as u8
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_deterministic_clear(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
) {
    (*handle).0.deterministic_clear((*txn).as_txn());
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_deterministic_key(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    index: u32,
    key: *mut u8,
) {
    let result = (*handle).0.deterministic_key((*txn).as_txn(), index);
    copy_raw_key_bytes(result, key);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_find(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    account: *const u8,
) -> *mut LmdbIteratorHandle {
    let mut iterator = (*handle)
        .0
        .find((*txn).as_txn(), &Account::from_ptr(account));
    to_lmdb_iterator_handle(iterator.as_mut())
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_valid_public_key(
    handle: *mut LmdbWalletStoreHandle,
    key: *const u8,
) -> bool {
    (*handle).0.valid_public_key(&PublicKey::from_ptr(key))
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_exists(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    key: *const u8,
) -> bool {
    (*handle)
        .0
        .exists((*txn).as_txn(), &PublicKey::from_ptr(key))
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_deterministic_insert(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    key: *mut u8,
) {
    let result = (*handle).0.deterministic_insert((*txn).as_txn());
    copy_public_key_bytes(&result, key);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_deterministic_insert_at(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    index: u32,
    key: *mut u8,
) {
    let result = (*handle).0.deterministic_insert_at((*txn).as_txn(), index);
    copy_public_key_bytes(&result, key);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_version(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
) -> u32 {
    (*handle).0.version((*txn).as_txn())
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_attempt_password(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    password: *const c_char,
) -> bool {
    let password = CStr::from_ptr(password).to_str().unwrap();
    (*handle).0.attempt_password((*txn).as_txn(), password)
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_lock(handle: *mut LmdbWalletStoreHandle) {
    (*handle).0.lock();
}

pub struct AccountArrayHandle(Box<Vec<[u8; 32]>>);

#[repr(C)]
pub struct AccountArrayDto {
    pub accounts: *const [u8; 32],
    pub count: usize,
    pub handle: *mut AccountArrayHandle,
}

#[no_mangle]
pub unsafe extern "C" fn rsn_account_array_destroy(dto: *mut AccountArrayDto) {
    drop(Box::from_raw((*dto).handle))
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_accounts(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    result: *mut AccountArrayDto,
) {
    let accounts = (*handle).0.accounts((*txn).as_txn());
    let handle = Box::new(AccountArrayHandle(Box::new(
        accounts.iter().map(|a| *a.as_bytes()).collect(),
    )));
    (*result).accounts = handle.0.as_ptr();
    (*result).count = handle.0.len();
    (*result).handle = Box::into_raw(handle);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_representative(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    account: *mut u8,
) {
    let rep = (*handle).0.representative((*txn).as_txn());
    copy_account_bytes(rep, account);
}

#[no_mangle]
pub unsafe extern "C" fn rsn_lmdb_wallet_store_representative_set(
    handle: *mut LmdbWalletStoreHandle,
    txn: *mut TransactionHandle,
    representative: *const u8,
) {
    (*handle)
        .0
        .representative_set((*txn).as_txn(), &Account::from_ptr(representative));
}
