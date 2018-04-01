#include <unistd.h>  // sysconf() - get CPU count
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#include "php_rocksdb.h"
#include "src/rocksdb.h"
#include "rocksdb/c.h"


// Default DB path
const char DBPath[] = "/tmp/rocksdb";
const char DBBackupPath[] = "/tmp/rocksdb_backup";
rocksdb_t *db;
rocksdb_backup_engine_t *be;
rocksdb_options_t *options;
rocksdb_writeoptions_t *writeoptions;
rocksdb_readoptions_t *readoptions;
rocksdb_restore_options_t *restore_options;

#define THROW_ERROR()  if (err) {   \
    assert(!err);                   \
  } else {                          \
    RETURN_BOOL(true);              \
  }  

ZEND_BEGIN_ARG_INFO(rocksdb_get_arg_info, 0)
   ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(rocksdb_put_arg_info, 0)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()


PHP_METHOD(RocksDB, __construct)
{
    if ((ZEND_NUM_ARGS() != 0)) {
        WRONG_PARAM_COUNT;
    }
    options = rocksdb_options_create();
    rocksdb_options_increase_parallelism(options, (int)(sysconf(_SC_NPROCESSORS_ONLN)));
    rocksdb_options_optimize_level_style_compaction(options, 0);
    rocksdb_options_set_create_if_missing(options, 1);
}

PHP_METHOD(RocksDB, connect)
{
  char *err;
  db = rocksdb_open(options, DBPath, &err);
  THROW_ERROR()
}

PHP_METHOD(RocksDB, enableBackup)
{
  char *err;
  be = rocksdb_backup_engine_open(options, DBBackupPath, &err);
  THROW_ERROR()
}

PHP_METHOD(RocksDB, newBackup)
{
  char *err;
  rocksdb_backup_engine_create_new_backup(be, db, &err);
  THROW_ERROR()
}

PHP_METHOD(RocksDB, restoreLastBackup)
{
  char *err = NULL;
  restore_options = rocksdb_restore_options_create();
  rocksdb_backup_engine_restore_db_from_latest_backup(be, DBPath, DBPath, restore_options, &err);
  THROW_ERROR()
}

PHP_METHOD(RocksDB, put)
{
  char *err, *c_key, *c_value;
  zend_string *key, *value;
  ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_STR(key)
    Z_PARAM_STR(value)
  ZEND_PARSE_PARAMETERS_END();

  c_key = ZSTR_VAL(key);
  c_value = ZSTR_VAL(value);
  writeoptions = rocksdb_writeoptions_create();
  rocksdb_put(db, writeoptions, c_key, strlen(c_key), c_value, strlen(c_value) + 1, &err);
  THROW_ERROR()
}

PHP_METHOD(RocksDB, get)
{
  size_t len;
  char *err,  *c_key, *returned_value;
  zend_string *key;
  ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STR(key)
  ZEND_PARSE_PARAMETERS_END();
  readoptions = rocksdb_readoptions_create();
  c_key = ZSTR_VAL(key);
  returned_value = rocksdb_get(db, readoptions, c_key, strlen(c_key), &len, &err);
  if (returned_value != NULL) {
    RETURN_STR(zend_string_init(returned_value, len, 0));
  } else {
    RETURN_BOOL(false);   
  }
 
}

PHP_METHOD(RocksDB, close)
{
  if (writeoptions != NULL) {
    rocksdb_writeoptions_destroy(writeoptions);
  }
  
  if (readoptions != NULL) {
    rocksdb_readoptions_destroy(readoptions); 
  }
  
  if (options != NULL) {
    rocksdb_options_destroy(options);
  }
  
  if (be != NULL) {
    rocksdb_backup_engine_close(be); 
  } 
  if (restore_options != NULL) {
     rocksdb_restore_options_destroy(restore_options);
  }

  if (db != NULL) {
    rocksdb_close(db);
  }

  RETURN_BOOL(true);
}

static zend_function_entry rocksdb_connector_methods[] = {
    PHP_ME(RocksDB, __construct, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(RocksDB, connect, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(RocksDB, put, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(RocksDB, get, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(RocksDB, close, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(RocksDB, enableBackup, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(RocksDB, newBackup, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(RocksDB, restoreLastBackup, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

PHP_ROCKSDB_STARTUP_FUNCTION(RocksDB) {
    zend_class_entry rocksdb_connector_ce;
    INIT_CLASS_ENTRY(rocksdb_connector_ce, "RocksDB\\RDB", rocksdb_connector_methods);
    rc_ce = zend_register_internal_class(&rocksdb_connector_ce TSRMLS_CC);
    return SUCCESS;
}