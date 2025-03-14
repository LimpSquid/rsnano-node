use crate::config::NetworkConstants;

pub struct NodeConstants {
    pub backup_interval_m: i64,
    pub search_pending_interval_s: i64,
    pub unchecked_cleaning_interval_m: i64,
    pub process_confirmed_interval_ms: i64,

    /** The maximum amount of samples for a 2 week period on live or 1 day on beta */
    pub max_weight_samples: u64,
    pub weight_period: u64,
}

impl NodeConstants {
    pub fn new(network_constants: &NetworkConstants) -> Self {
        Self {
            backup_interval_m: 5,
            search_pending_interval_s: if network_constants.is_dev_network() {
                1
            } else {
                5 * 60
            },
            unchecked_cleaning_interval_m: 30,
            process_confirmed_interval_ms: if network_constants.is_dev_network() {
                50
            } else {
                500
            },
            max_weight_samples: if network_constants.is_live_network()
                || network_constants.is_test_network()
            {
                4032
            } else {
                288
            },
            weight_period: 5 * 60,
        }
    }
}
