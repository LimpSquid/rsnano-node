use std::sync::Arc;

use crate::{
    datastore::{
        lmdb::{MDB_NOTFOUND, MDB_SUCCESS},
        FrontierStore, WriteTransaction,
    },
    Account, BlockHash,
};

use super::{
    assert_success, get_raw_lmdb_txn, mdb_del, mdb_get, mdb_put, LmdbEnv, LmdbIterator, MdbVal,
};

pub struct LmdbFrontierStore {
    env: Arc<LmdbEnv>,
    pub table_handle: u32,
}

impl LmdbFrontierStore {
    pub fn new(env: Arc<LmdbEnv>) -> Self {
        Self {
            env,
            table_handle: 0,
        }
    }
}

impl FrontierStore for LmdbFrontierStore {
    fn put(&self, txn: &dyn WriteTransaction, hash: &BlockHash, account: &Account) {
        let status = unsafe {
            mdb_put(
                get_raw_lmdb_txn(txn.as_transaction()),
                self.table_handle,
                &mut MdbVal::from(hash),
                &mut MdbVal::from(account),
                0,
            )
        };
        assert_success(status);
    }

    fn get(&self, txn: &dyn crate::datastore::Transaction, hash: &BlockHash) -> Account {
        let mut value = MdbVal::new();
        let status = unsafe {
            mdb_get(
                get_raw_lmdb_txn(txn),
                self.table_handle,
                &mut MdbVal::from(hash),
                &mut value,
            )
        };
        assert!(status == MDB_SUCCESS || status == MDB_NOTFOUND);
        if status == MDB_SUCCESS {
            Account::from_slice(value.as_slice()).unwrap_or_default()
        } else {
            Account::new()
        }
    }

    fn del(&self, txn: &dyn WriteTransaction, hash: &BlockHash) {
        let status = unsafe {
            mdb_del(
                get_raw_lmdb_txn(txn.as_transaction()),
                self.table_handle,
                &mut MdbVal::from(hash),
                None,
            )
        };
        assert_success(status);
    }

    fn begin(
        &self,
        txn: &dyn crate::datastore::Transaction,
    ) -> Box<dyn crate::datastore::DbIterator<BlockHash, Account>> {
        Box::new(LmdbIterator::new(txn, self.table_handle, None, true))
    }

    fn begin_at_hash(
        &self,
        txn: &dyn crate::datastore::Transaction,
        hash: &BlockHash,
    ) -> Box<dyn crate::datastore::DbIterator<BlockHash, Account>> {
        Box::new(LmdbIterator::new(txn, self.table_handle, Some(hash), true))
    }
}
