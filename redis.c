/* -*- Mode: C; tab-width: 4 -*- */
/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2009 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Original author: Alfonso Jimenez <yo@alfonsojimenez.com>             |
  | Maintainer: Nicolas Favre-Felix <n.favre-felix@owlient.eu>           |
  | Maintainer: Nasreddine Bouafif <n.bouafif@owlient.eu>                |
  | Maintainer: Michael Grunder <michael.grunder@gmail.com>              |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common.h"
#include "ext/standard/info.h"
#include "php_redis.h"
#include "redis_commands.h"
#include "redis_array.h"
#include "redis_cluster.h"
#include <zend_exceptions.h>

#ifdef PHP_SESSION
#include "ext/session/php_session.h"
#endif

#include "library.h"

#ifdef HAVE_REDIS_ZSTD
#include <zstd.h>
#endif

#ifdef PHP_SESSION
extern ps_module ps_mod_redis;
extern ps_module ps_mod_redis_cluster;
#endif

extern zend_class_entry *redis_array_ce;
extern zend_class_entry *redis_cluster_ce;
extern zend_class_entry *redis_cluster_exception_ce;

zend_class_entry *redis_ce;
zend_class_entry *redis_exception_ce;

extern int le_cluster_slot_cache;

extern zend_function_entry redis_array_functions[];
extern zend_function_entry redis_cluster_functions[];

int le_redis_pconnect;

PHP_INI_BEGIN()
    /* redis arrays */
    PHP_INI_ENTRY("awesomeredis.arrays.algorithm", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.auth", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.autorehash", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.connecttimeout", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.distributor", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.functions", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.hosts", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.index", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.lazyconnect", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.names", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.pconnect", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.previous", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.readtimeout", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.retryinterval", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.arrays.consistent", "0", PHP_INI_ALL, NULL)

    /* redis cluster */
    PHP_INI_ENTRY("awesomeredis.clusters.cache_slots", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.clusters.auth", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.clusters.persistent", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.clusters.read_timeout", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.clusters.seeds", "", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.clusters.timeout", "0", PHP_INI_ALL, NULL)

    /* redis pconnect */
    PHP_INI_ENTRY("awesomeredis.pconnect.pooling_enabled", "1", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.pconnect.connection_limit", "0", PHP_INI_ALL, NULL)

    /* redis session */
    PHP_INI_ENTRY("awesomeredis.session.locking_enabled", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.session.lock_expire", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.session.lock_retries", "10", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("awesomeredis.session.lock_wait_time", "2000", PHP_INI_ALL, NULL)
PHP_INI_END()

/** {{{ Argument info for commands in redis 1.0 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_connect, 0, 0, 1)
    ZEND_ARG_INFO(0, host)
    ZEND_ARG_INFO(0, port)
    ZEND_ARG_INFO(0, timeout)
    ZEND_ARG_INFO(0, retry_interval)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_info, 0, 0, 0)
    ZEND_ARG_INFO(0, option)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_multi, 0, 0, 0)
    ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_client, 0, 0, 1)
    ZEND_ARG_INFO(0, cmd)
    ZEND_ARG_VARIADIC_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_config, 0, 0, 2)
    ZEND_ARG_INFO(0, cmd)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_flush, 0, 0, 0)
    ZEND_ARG_INFO(0, async)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_pubsub, 0, 0, 1)
    ZEND_ARG_INFO(0, cmd)
    ZEND_ARG_VARIADIC_INFO(0, args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_slowlog, 0, 0, 1)
    ZEND_ARG_INFO(0, arg)
    ZEND_ARG_INFO(0, option)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_pconnect, 0, 0, 1)
    ZEND_ARG_INFO(0, host)
    ZEND_ARG_INFO(0, port)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_mget, 0, 0, 1)
    ZEND_ARG_ARRAY_INFO(0, keys, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_exists, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_VARIADIC_INFO(0, other_keys)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_del, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_VARIADIC_INFO(0, other_keys)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_keys, 0, 0, 1)
    ZEND_ARG_INFO(0, pattern)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_generic_sort, 0, 0, 1)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, pattern)
    ZEND_ARG_INFO(0, get)
    ZEND_ARG_INFO(0, start)
    ZEND_ARG_INFO(0, end)
    ZEND_ARG_INFO(0, getList)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lrem, 0, 0, 3)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, value)
    ZEND_ARG_INFO(0, count)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_auth, 0, 0, 1)
    ZEND_ARG_INFO(0, password)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_select, 0, 0, 1)
    ZEND_ARG_INFO(0, dbindex)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_move, 0, 0, 2)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, dbindex)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_slaveof, 0, 0, 0)
    ZEND_ARG_INFO(0, host)
    ZEND_ARG_INFO(0, port)
ZEND_END_ARG_INFO()

/* }}} */

ZEND_BEGIN_ARG_INFO_EX(arginfo_migrate, 0, 0, 5)
    ZEND_ARG_INFO(0, host)
    ZEND_ARG_INFO(0, port)
    ZEND_ARG_INFO(0, key)
    ZEND_ARG_INFO(0, db)
    ZEND_ARG_INFO(0, timeout)
    ZEND_ARG_INFO(0, copy)
    ZEND_ARG_INFO(0, replace)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wait, 0, 0, 2)
    ZEND_ARG_INFO(0, numslaves)
    ZEND_ARG_INFO(0, timeout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_script, 0, 0, 1)
    ZEND_ARG_INFO(0, cmd)
    ZEND_ARG_VARIADIC_INFO(0, args)
ZEND_END_ARG_INFO()

/**
 * Argument info for the SCAN proper
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_scan, 0, 0, 1)
    ZEND_ARG_INFO(1, i_iterator)
    ZEND_ARG_INFO(0, str_pattern)
    ZEND_ARG_INFO(0, i_count)
ZEND_END_ARG_INFO()

/**
 * Argument info for key scanning
 */
ZEND_BEGIN_ARG_INFO_EX(arginfo_kscan, 0, 0, 2)
    ZEND_ARG_INFO(0, str_key)
    ZEND_ARG_INFO(1, i_iterator)
    ZEND_ARG_INFO(0, str_pattern)
    ZEND_ARG_INFO(0, i_count)
ZEND_END_ARG_INFO()

static zend_function_entry redis_functions[] = {
     PHP_ME(AwesomeRedis, __construct, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, __destruct, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, _prefix, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, _serialize, arginfo_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, _unserialize, arginfo_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, append, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, auth, arginfo_auth, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, bgSave, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, bgrewriteaof, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, bitcount, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, bitop, arginfo_bitop, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, bitpos, arginfo_bitpos, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, blPop, arginfo_blrpop, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, brPop, arginfo_blrpop, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, brpoplpush, arginfo_brpoplpush, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, bzPopMax, arginfo_blrpop, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, bzPopMin, arginfo_blrpop, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, clearLastError, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, client, arginfo_client, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, close, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, command, arginfo_command, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, config, arginfo_config, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, connect, arginfo_connect, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, dbSize, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, debug, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, decr, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, decrBy, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, del, arginfo_del, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, discard, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, dump, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, echo, arginfo_echo, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, eval, arginfo_eval, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, evalsha, arginfo_evalsha, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, exec, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, exists, arginfo_exists, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, expire, arginfo_expire, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, expireAt, arginfo_key_timestamp, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, flushAll, arginfo_flush, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, flushDB, arginfo_flush, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, geoadd, arginfo_geoadd, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, geodist, arginfo_geodist, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, geohash, arginfo_key_members, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, geopos, arginfo_key_members, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, georadius, arginfo_georadius, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, georadius_ro, arginfo_georadius, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, georadiusbymember, arginfo_georadiusbymember, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, georadiusbymember_ro, arginfo_georadiusbymember, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, get, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getAuth, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getBit, arginfo_key_offset, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getDBNum, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getHost, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getLastError, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getMode, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getOption, arginfo_getoption, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getPersistentID, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getPort, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getRange, arginfo_key_start_end, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getReadTimeout, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getSet, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, getTimeout, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hDel, arginfo_key_members, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hExists, arginfo_key_member, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hGet, arginfo_key_member, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hGetAll, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hIncrBy, arginfo_key_member_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hIncrByFloat, arginfo_key_member_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hKeys, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hLen, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hMget, arginfo_hmget, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hMset, arginfo_hmset, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hSet, arginfo_key_member_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hSetNx, arginfo_key_member_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hStrLen, arginfo_key_member, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hVals, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, hscan, arginfo_kscan, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, incr, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, incrBy, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, incrByFloat, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, info, arginfo_info, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, isConnected, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, keys, arginfo_keys, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lInsert, arginfo_linsert, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lLen, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lPop, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lPush, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lPushx, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lSet, arginfo_lset, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lastSave, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lindex, arginfo_lindex, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lrange, arginfo_key_start_end, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, lrem, arginfo_lrem, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, ltrim, arginfo_ltrim, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, mget, arginfo_mget, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, migrate, arginfo_migrate, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, move, arginfo_move, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, mset, arginfo_pairs, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, msetnx, arginfo_pairs, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, multi, arginfo_multi, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, object, arginfo_object, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pconnect, arginfo_pconnect, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, persist, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pexpire, arginfo_key_timestamp, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pexpireAt, arginfo_key_timestamp, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pfadd, arginfo_pfadd, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pfcount, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pfmerge, arginfo_pfmerge, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, ping, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pipeline, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, psetex, arginfo_key_expire_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, psubscribe, arginfo_psubscribe, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pttl, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, publish, arginfo_publish, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, pubsub, arginfo_pubsub, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, punsubscribe, arginfo_punsubscribe, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, rPop, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, rPush, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, rPushx, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, randomKey, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, rawcommand, arginfo_rawcommand, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, rename, arginfo_key_newkey, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, renameNx, arginfo_key_newkey, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, restore, arginfo_restore, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, role, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, rpoplpush, arginfo_rpoplpush, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sAdd, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sAddArray, arginfo_sadd_array, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sDiff, arginfo_nkeys, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sDiffStore, arginfo_dst_nkeys, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sInter, arginfo_nkeys, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sInterStore, arginfo_dst_nkeys, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sMembers, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sMove, arginfo_smove, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sPop, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sRandMember, arginfo_srand_member, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sUnion, arginfo_nkeys, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sUnionStore, arginfo_dst_nkeys, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, save, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, scan, arginfo_scan, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, scard, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, script, arginfo_script, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, select, arginfo_select, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, set, arginfo_set, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, setBit, arginfo_key_offset_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, setOption, arginfo_setoption, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, setRange, arginfo_key_offset_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, setex, arginfo_key_expire_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, setnx, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sismember, arginfo_key_value, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, slaveof, arginfo_slaveof, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, slowlog, arginfo_slowlog, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sort, arginfo_sort, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sortAsc, arginfo_generic_sort, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_ME(AwesomeRedis, sortAscAlpha, arginfo_generic_sort, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_ME(AwesomeRedis, sortDesc, arginfo_generic_sort, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_ME(AwesomeRedis, sortDescAlpha, arginfo_generic_sort, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_ME(AwesomeRedis, srem, arginfo_key_members, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, sscan, arginfo_kscan, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, strlen, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, subscribe, arginfo_subscribe, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, swapdb, arginfo_swapdb, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, time, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, ttl, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, type, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, unlink, arginfo_nkeys, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, unsubscribe, arginfo_unsubscribe, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, unwatch, arginfo_void, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, wait, arginfo_wait, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, watch, arginfo_watch, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xack, arginfo_xack, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xadd, arginfo_xadd, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xclaim, arginfo_xclaim, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xdel, arginfo_xdel, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xgroup, arginfo_xgroup, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xinfo, arginfo_xinfo, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xlen, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xpending, arginfo_xpending, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xrange, arginfo_xrange, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xread, arginfo_xread, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xreadgroup, arginfo_xreadgroup, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xrevrange, arginfo_xrange, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, xtrim, arginfo_xtrim, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zAdd, arginfo_zadd, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zCard, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zCount, arginfo_key_min_max, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zIncrBy, arginfo_zincrby, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zLexCount, arginfo_key_min_max, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zPopMax, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zPopMin, arginfo_key, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRange, arginfo_zrange, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRangeByLex, arginfo_zrangebylex, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRangeByScore, arginfo_zrangebyscore, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRank, arginfo_key_member, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRem, arginfo_key_members, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRemRangeByLex, arginfo_key_min_max, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRemRangeByRank, arginfo_key_start_end, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRemRangeByScore, arginfo_key_min_max, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRevRange, arginfo_zrange, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRevRangeByLex, arginfo_zrangebylex, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRevRangeByScore, arginfo_zrangebyscore, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zRevRank, arginfo_key_member, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zScore, arginfo_key_member, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zinterstore, arginfo_zstore, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zscan, arginfo_kscan, ZEND_ACC_PUBLIC)
     PHP_ME(AwesomeRedis, zunionstore, arginfo_zstore, ZEND_ACC_PUBLIC)

     /* Mark all of these aliases deprecated.  They aren't actual Redis commands. */
     PHP_MALIAS(AwesomeRedis, delete, del, arginfo_del, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, evaluate, eval, arginfo_eval, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, evaluateSha, evalsha, arginfo_evalsha, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, getKeys, keys, arginfo_keys, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, getMultiple, mget, arginfo_mget, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, lGet, lindex, arginfo_lindex, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, lGetRange, lrange, arginfo_key_start_end, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, lRemove, lrem, arginfo_lrem, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, lSize, lLen, arginfo_key, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, listTrim, ltrim, arginfo_ltrim, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, open, connect, arginfo_connect, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, popen, pconnect, arginfo_pconnect, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, renameKey, rename, arginfo_key_newkey, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, sContains, sismember, arginfo_key_value, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, sGetMembers, sMembers, arginfo_key, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, sRemove, srem, arginfo_key_members, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, sSize, scard, arginfo_key, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, sendEcho, echo, arginfo_echo, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, setTimeout, expire, arginfo_expire, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, substr, getRange, arginfo_key_start_end, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zDelete, zRem, arginfo_key_members, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zDeleteRangeByRank, zRemRangeByRank, arginfo_key_min_max, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zDeleteRangeByScore, zRemRangeByScore, arginfo_key_min_max, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zInter, zinterstore, arginfo_zstore, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zRemove, zRem, arginfo_key_members, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zRemoveRangeByScore, zRemRangeByScore, arginfo_key_min_max, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zReverseRange, zRevRange, arginfo_zrange, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zSize, zCard, arginfo_key, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_MALIAS(AwesomeRedis, zUnion, zunionstore, arginfo_zstore, ZEND_ACC_PUBLIC | ZEND_ACC_DEPRECATED)
     PHP_FE_END
};

static const zend_module_dep redis_deps[] = {
#ifdef HAVE_REDIS_IGBINARY
     ZEND_MOD_REQUIRED("igbinary")
#endif
#ifdef HAVE_REDIS_MSGPACK
     ZEND_MOD_REQUIRED("msgpack")
#endif
#ifdef HAVE_REDIS_JSON
     ZEND_MOD_REQUIRED("json")
#endif
#ifdef PHP_SESSION
     ZEND_MOD_REQUIRED("session")
#endif
     ZEND_MOD_END
};

zend_module_entry redis_module_entry = {
     STANDARD_MODULE_HEADER_EX,
     NULL,
     redis_deps,
     "awesomeredis",
     NULL,
     PHP_MINIT(redis),
     PHP_MSHUTDOWN(redis),
     NULL,
     NULL,
     PHP_MINFO(redis),
     PHP_REDIS_VERSION,
     STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_REDIS
ZEND_GET_MODULE(redis)
#endif

zend_object_handlers redis_object_handlers;

/* Send a static DISCARD in case we're in MULTI mode. */
static int
redis_send_discard(RedisSock *redis_sock)
{
    int result = FAILURE;
    char *cmd, *resp;
    int resp_len, cmd_len;

    /* format our discard command */
    cmd_len = REDIS_SPPRINTF(&cmd, "DISCARD", "");

    /* send our DISCARD command */
    if (redis_sock_write(redis_sock, cmd, cmd_len) >= 0 &&
       (resp = redis_sock_read(redis_sock,&resp_len)) != NULL)
    {
        /* success if we get OK */
        result = (resp_len == 3 && strncmp(resp,"+OK", 3) == 0) ? SUCCESS:FAILURE;

        /* free our response */
        efree(resp);
    }

    /* free our command */
    efree(cmd);

    /* return success/failure */
    return result;
}

static void
free_reply_callbacks(RedisSock *redis_sock)
{
    fold_item *fi;

    for (fi = redis_sock->head; fi; ) {
        fold_item *fi_next = fi->next;
        free(fi);
        fi = fi_next;
    }
    redis_sock->head = NULL;
    redis_sock->current = NULL;
}

/* Passthru for destroying cluster cache */
static void cluster_cache_dtor(zend_resource *rsrc) {
    if (rsrc->ptr) {
        cluster_cache_free(rsrc->ptr);
    }
}

void
free_redis_object(zend_object *object)
{
    redis_object *redis = (redis_object *)((char *)(object) - XtOffsetOf(redis_object, std));

    zend_object_std_dtor(&redis->std);
    if (redis->sock) {
        redis_sock_disconnect(redis->sock, 0);
        redis_free_socket(redis->sock);
    }
}

zend_object *
create_redis_object(zend_class_entry *ce)
{
    redis_object *redis = ecalloc(1, sizeof(redis_object) + zend_object_properties_size(ce));

    redis->sock = NULL;

    zend_object_std_init(&redis->std, ce);
    object_properties_init(&redis->std, ce);

    memcpy(&redis_object_handlers, zend_get_std_object_handlers(), sizeof(redis_object_handlers));
    redis_object_handlers.offset = XtOffsetOf(redis_object, std);
    redis_object_handlers.free_obj = free_redis_object;
    redis->std.handlers = &redis_object_handlers;

    return &redis->std;
}

static zend_always_inline RedisSock *
redis_sock_get_instance(zval *id, int no_throw)
{
    redis_object *redis;

    if (Z_TYPE_P(id) == IS_OBJECT) {
        redis = PHPREDIS_GET_OBJECT(redis_object, id);
        if (redis->sock) {
            return redis->sock;
        }
    }
    // Throw an exception unless we've been requested not to
    if (!no_throw) {
        REDIS_THROW_EXCEPTION("Redis server went away", 0);
    }
    return NULL;
}

/**
 * redis_sock_get
 */
PHP_REDIS_API RedisSock *
redis_sock_get(zval *id, int no_throw)
{
    RedisSock *redis_sock;

    if ((redis_sock = redis_sock_get_instance(id, no_throw)) == NULL) {
        return NULL;
    }

    if (redis_sock_server_open(redis_sock) < 0) {
        if (!no_throw) {
            char *errmsg = NULL;
            if (redis_sock->port < 0) {
                spprintf(&errmsg, 0, "Redis server %s went away", ZSTR_VAL(redis_sock->host));
            } else {
                spprintf(&errmsg, 0, "Redis server %s:%d went away", ZSTR_VAL(redis_sock->host), redis_sock->port);
            }
            REDIS_THROW_EXCEPTION(errmsg, 0);
            efree(errmsg);
        }
        return NULL;
    }

    return redis_sock;
}

/**
 * redis_sock_get_direct
 * Returns our attached RedisSock pointer if we're connected
 */
PHP_REDIS_API RedisSock *redis_sock_get_connected(INTERNAL_FUNCTION_PARAMETERS) {
    zval *object;
    RedisSock *redis_sock;

    // If we can't grab our object, or get a socket, or we're not connected,
    // return NULL
    if((zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "O",
       &object, redis_ce) == FAILURE) ||
       (redis_sock = redis_sock_get(object, 1)) == NULL ||
       redis_sock->status != REDIS_SOCK_STATUS_CONNECTED)
    {
        return NULL;
    }

    /* Return our socket */
    return redis_sock;
}

/* Redis and RedisCluster objects share serialization/prefixing settings so
 * this is a generic function to add class constants to either */
static void add_class_constants(zend_class_entry *ce, int is_cluster) {
    zend_declare_class_constant_long(ce, ZEND_STRL("REDIS_NOT_FOUND"), REDIS_NOT_FOUND);
    zend_declare_class_constant_long(ce, ZEND_STRL("REDIS_STRING"), REDIS_STRING);
    zend_declare_class_constant_long(ce, ZEND_STRL("REDIS_SET"), REDIS_SET);
    zend_declare_class_constant_long(ce, ZEND_STRL("REDIS_LIST"), REDIS_LIST);
    zend_declare_class_constant_long(ce, ZEND_STRL("REDIS_ZSET"), REDIS_ZSET);
    zend_declare_class_constant_long(ce, ZEND_STRL("REDIS_HASH"), REDIS_HASH);
    zend_declare_class_constant_long(ce, ZEND_STRL("REDIS_STREAM"), REDIS_STREAM);

    /* Cluster doesn't support pipelining at this time */
    if(!is_cluster) {
        zend_declare_class_constant_long(ce, ZEND_STRL("PIPELINE"), PIPELINE);
    }

    /* Add common mode constants */
    zend_declare_class_constant_long(ce, ZEND_STRL("ATOMIC"), ATOMIC);
    zend_declare_class_constant_long(ce, ZEND_STRL("MULTI"), MULTI);

    /* options */
    zend_declare_class_constant_long(ce, ZEND_STRL("OPT_SERIALIZER"), REDIS_OPT_SERIALIZER);
    zend_declare_class_constant_long(ce, ZEND_STRL("OPT_PREFIX"), REDIS_OPT_PREFIX);
    zend_declare_class_constant_long(ce, ZEND_STRL("OPT_READ_TIMEOUT"), REDIS_OPT_READ_TIMEOUT);
    zend_declare_class_constant_long(ce, ZEND_STRL("OPT_TCP_KEEPALIVE"), REDIS_OPT_TCP_KEEPALIVE);
    zend_declare_class_constant_long(ce, ZEND_STRL("OPT_COMPRESSION"), REDIS_OPT_COMPRESSION);
    zend_declare_class_constant_long(ce, ZEND_STRL("OPT_REPLY_LITERAL"), REDIS_OPT_REPLY_LITERAL);
    zend_declare_class_constant_long(ce, ZEND_STRL("OPT_COMPRESSION_LEVEL"), REDIS_OPT_COMPRESSION_LEVEL);

    /* serializer */
    zend_declare_class_constant_long(ce, ZEND_STRL("SERIALIZER_NONE"), REDIS_SERIALIZER_NONE);
    zend_declare_class_constant_long(ce, ZEND_STRL("SERIALIZER_PHP"), REDIS_SERIALIZER_PHP);
#ifdef HAVE_REDIS_IGBINARY
    zend_declare_class_constant_long(ce, ZEND_STRL("SERIALIZER_IGBINARY"), REDIS_SERIALIZER_IGBINARY);
#endif
#ifdef HAVE_REDIS_MSGPACK
    zend_declare_class_constant_long(ce, ZEND_STRL("SERIALIZER_MSGPACK"), REDIS_SERIALIZER_MSGPACK);
#endif
    zend_declare_class_constant_long(ce, ZEND_STRL("SERIALIZER_JSON"), REDIS_SERIALIZER_JSON);

    /* compression */
    zend_declare_class_constant_long(ce, ZEND_STRL("COMPRESSION_NONE"), REDIS_COMPRESSION_NONE);
#ifdef HAVE_REDIS_LZF
    zend_declare_class_constant_long(ce, ZEND_STRL("COMPRESSION_LZF"), REDIS_COMPRESSION_LZF);
#endif
#ifdef HAVE_REDIS_ZSTD
    zend_declare_class_constant_long(ce, ZEND_STRL("COMPRESSION_ZSTD"), REDIS_COMPRESSION_ZSTD);
    zend_declare_class_constant_long(ce, ZEND_STRL("COMPRESSION_ZSTD_MIN"), 1);
#ifdef ZSTD_CLEVEL_DEFAULT
    zend_declare_class_constant_long(ce, ZEND_STRL("COMPRESSION_ZSTD_DEFAULT"), ZSTD_CLEVEL_DEFAULT);
#else
    zend_declare_class_constant_long(ce, ZEND_STRL("COMPRESSION_ZSTD_DEFAULT"), 3);
#endif
    zend_declare_class_constant_long(ce, ZEND_STRL("COMPRESSION_ZSTD_MAX"), ZSTD_maxCLevel());
#endif

    /* scan options*/
    zend_declare_class_constant_long(ce, ZEND_STRL("OPT_SCAN"), REDIS_OPT_SCAN);
    zend_declare_class_constant_long(ce, ZEND_STRL("SCAN_RETRY"), REDIS_SCAN_RETRY);
    zend_declare_class_constant_long(ce, ZEND_STRL("SCAN_NORETRY"), REDIS_SCAN_NORETRY);

    /* Cluster option to allow for slave failover */
    if (is_cluster) {
        zend_declare_class_constant_long(ce, ZEND_STRL("OPT_SLAVE_FAILOVER"), REDIS_OPT_FAILOVER);
        zend_declare_class_constant_long(ce, ZEND_STRL("FAILOVER_NONE"), REDIS_FAILOVER_NONE);
        zend_declare_class_constant_long(ce, ZEND_STRL("FAILOVER_ERROR"), REDIS_FAILOVER_ERROR);
        zend_declare_class_constant_long(ce, ZEND_STRL("FAILOVER_DISTRIBUTE"), REDIS_FAILOVER_DISTRIBUTE);
        zend_declare_class_constant_long(ce, ZEND_STRL("FAILOVER_DISTRIBUTE_SLAVES"), REDIS_FAILOVER_DISTRIBUTE_SLAVES);
    }

    zend_declare_class_constant_stringl(ce, "AFTER", 5, "after", 5);
    zend_declare_class_constant_stringl(ce, "BEFORE", 6, "before", 6);
}

static ZEND_RSRC_DTOR_FUNC(redis_connections_pool_dtor)
{
    if (res->ptr) {
        ConnectionPool *p = res->ptr;
        zend_llist_destroy(&p->list);
        pefree(res->ptr, 1);
    }
}

/**
 * PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(redis)
{
    struct timeval tv;

    zend_class_entry redis_class_entry;
    zend_class_entry redis_array_class_entry;
    zend_class_entry redis_cluster_class_entry;
    zend_class_entry redis_exception_class_entry;
    zend_class_entry redis_cluster_exception_class_entry;

    zend_class_entry *exception_ce = NULL;

    /* Seed random generator (for RedisCluster failover) */
    gettimeofday(&tv, NULL);
    srand(tv.tv_usec * tv.tv_sec);

    REGISTER_INI_ENTRIES();

    /* Redis class */
    INIT_CLASS_ENTRY(redis_class_entry, "AwesomeRedis", redis_functions);
    redis_ce = zend_register_internal_class(&redis_class_entry);
    redis_ce->create_object = create_redis_object;

    /* RedisArray class */
    INIT_CLASS_ENTRY(redis_array_class_entry, "RedisArray", redis_array_functions);
    redis_array_ce = zend_register_internal_class(&redis_array_class_entry);
    redis_array_ce->create_object = create_redis_array_object;

    /* RedisCluster class */
    INIT_CLASS_ENTRY(redis_cluster_class_entry, "RedisCluster", redis_cluster_functions);
    redis_cluster_ce = zend_register_internal_class(&redis_cluster_class_entry);
    redis_cluster_ce->create_object = create_cluster_context;

    /* Register our cluster cache list item */
    le_cluster_slot_cache = zend_register_list_destructors_ex(NULL, cluster_cache_dtor,
                                                              "Redis cluster slot cache",
                                                              module_number);

    /* Base Exception class */
    exception_ce = zend_hash_str_find_ptr(CG(class_table), "RuntimeException", sizeof("RuntimeException") - 1);
    if (exception_ce == NULL) {
        exception_ce = zend_exception_get_default();
    }

    /* RedisException class */
    INIT_CLASS_ENTRY(redis_exception_class_entry, "RedisException", NULL);
    redis_exception_ce = zend_register_internal_class_ex(
        &redis_exception_class_entry,
        exception_ce);

    /* RedisClusterException class */
    INIT_CLASS_ENTRY(redis_cluster_exception_class_entry,
        "RedisClusterException", NULL);
    redis_cluster_exception_ce = zend_register_internal_class_ex(
        &redis_cluster_exception_class_entry, exception_ce);

    /* Add shared class constants to Redis and RedisCluster objects */
    add_class_constants(redis_ce, 0);
    add_class_constants(redis_cluster_ce, 1);

#ifdef PHP_SESSION
    php_session_register_module(&ps_mod_redis);
    php_session_register_module(&ps_mod_redis_cluster);
#endif

    /* Register resource destructors */
    le_redis_pconnect = zend_register_list_destructors_ex(NULL, redis_connections_pool_dtor,
        "phpredis persistent connections pool", module_number);

    return SUCCESS;
}

/**
 * PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(redis)
{
    return SUCCESS;
}

static const char *
get_available_serializers(void)
{
#ifdef HAVE_REDIS_JSON
    #ifdef HAVE_REDIS_IGBINARY
        #ifdef HAVE_REDIS_MSGPACK
            return "php, json, igbinary, msgpack";
        #else
            return "php, json, igbinary";
        #endif
    #else
        #ifdef HAVE_REDIS_MSGPACK
            return "php, json, msgpack";
        #else
            return "php, json";
        #endif
    #endif
#else
    #ifdef HAVE_REDIS_IGBINARY
        #ifdef HAVE_REDIS_MSGPACK
            return "php, igbinary, msgpack";
        #else
            return "php, igbinary";
        #endif
    #else
        #ifdef HAVE_REDIS_MSGPACK
            return "php, msgpack";
        #else
            return "php";
        #endif
    #endif
#endif
}

/**
 * PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(redis)
{
    smart_str names = {0,};

    php_info_print_table_start();
    php_info_print_table_header(2, "Redis Support", "enabled");
    php_info_print_table_row(2, "Redis Version", PHP_REDIS_VERSION);
#ifdef GIT_REVISION
    php_info_print_table_row(2, "Git revision", "$Id: " GIT_REVISION " $");
#endif
    php_info_print_table_row(2, "Available serializers", get_available_serializers());
#ifdef HAVE_REDIS_LZF
    smart_str_appends(&names, "lzf");
#endif
#ifdef HAVE_REDIS_ZSTD
    if (names.s) {
        smart_str_appends(&names, ", ");
    }
    smart_str_appends(&names, "zstd");
#endif
    if (names.s) {
        php_info_print_table_row(2, "Available compression", names.s->val);
    }
    smart_str_free(&names);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

/* {{{ proto Redis AwesomeRedis::__construct()
    Public constructor */
PHP_METHOD(AwesomeRedis, __construct)
{
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "") == FAILURE) {
        RETURN_FALSE;
    }
}
/* }}} */

/* {{{ proto Redis AwesomeRedis::__destruct()
    Public Destructor
 */
PHP_METHOD(AwesomeRedis,__destruct) {
    if(zend_parse_parameters(ZEND_NUM_ARGS(), "") == FAILURE) {
        RETURN_FALSE;
    }

    // Grab our socket
    RedisSock *redis_sock;
    if ((redis_sock = redis_sock_get_instance(getThis(), 1)) == NULL) {
        RETURN_FALSE;
    }

    // If we think we're in MULTI mode, send a discard
    if (IS_MULTI(redis_sock)) {
        if (!IS_PIPELINE(redis_sock) && redis_sock->stream) {
            // Discard any multi commands, and free any callbacks that have been
            // queued
            redis_send_discard(redis_sock);
        }
        free_reply_callbacks(redis_sock);
    }
}

/* {{{ proto boolean AwesomeRedis::connect(string host, int port [, double timeout [, long retry_interval]])
 */
PHP_METHOD(AwesomeRedis, connect)
{
    if (redis_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0) == FAILURE) {
        RETURN_FALSE;
    } else {
        RETURN_TRUE;
    }
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::pconnect(string host, int port [, double timeout])
 */
PHP_METHOD(AwesomeRedis, pconnect)
{
    if (redis_connect(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1) == FAILURE) {
        RETURN_FALSE;
    } else {
        RETURN_TRUE;
    }
}
/* }}} */

PHP_REDIS_API int
redis_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent)
{
    zval *object;
    char *host = NULL, *persistent_id = NULL;
    zend_long port = -1, retry_interval = 0;
    size_t host_len, persistent_id_len;
    double timeout = 0.0, read_timeout = 0.0;
    redis_object *redis;

#ifdef ZTS
    /* not sure how in threaded mode this works so disabled persistence at
     * first */
    persistent = 0;
#endif

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "Os|lds!ld", &object, redis_ce, &host,
                                     &host_len, &port, &timeout, &persistent_id,
                                     &persistent_id_len, &retry_interval,
                                     &read_timeout) == FAILURE)
    {
        return FAILURE;
    }

    /* Disregard persistent_id if we're not opening a persistent connection */
    if (!persistent) {
        persistent_id = NULL;
    }

    if (timeout < 0L || timeout > INT_MAX) {
        REDIS_THROW_EXCEPTION("Invalid connect timeout", 0);
        return FAILURE;
    }

    if (read_timeout < 0L || read_timeout > INT_MAX) {
        REDIS_THROW_EXCEPTION("Invalid read timeout", 0);
        return FAILURE;
    }

    if (retry_interval < 0L || retry_interval > INT_MAX) {
        REDIS_THROW_EXCEPTION("Invalid retry interval", 0);
        return FAILURE;
    }

    /* If it's not a unix socket, set to default */
    if(port == -1 && host_len && host[0] != '/') {
        port = 6379;
    }

    redis = PHPREDIS_GET_OBJECT(redis_object, object);
    /* if there is a redis sock already we have to remove it */
    if (redis->sock) {
        redis_sock_disconnect(redis->sock, 0);
        redis_free_socket(redis->sock);
    }

    redis->sock = redis_sock_create(host, host_len, port, timeout, read_timeout, persistent,
        persistent_id, retry_interval);

    if (redis_sock_server_open(redis->sock) < 0) {
        if (redis->sock->err) {
            REDIS_THROW_EXCEPTION(ZSTR_VAL(redis->sock->err), 0);
        }
        redis_free_socket(redis->sock);
        redis->sock = NULL;
        return FAILURE;
    }

    return SUCCESS;
}

/* {{{ proto long AwesomeRedis::bitop(string op, string key, ...) */
PHP_METHOD(AwesomeRedis, bitop)
{
    REDIS_PROCESS_CMD(bitop, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::bitcount(string key, [int start], [int end])
 */
PHP_METHOD(AwesomeRedis, bitcount)
{
    REDIS_PROCESS_CMD(bitcount, redis_long_response);
}
/* }}} */

/* {{{ proto integer AwesomeRedis::bitpos(string key, int bit, [int start, int end]) */
PHP_METHOD(AwesomeRedis, bitpos)
{
    REDIS_PROCESS_CMD(bitpos, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::close()
 */
PHP_METHOD(AwesomeRedis, close)
{
    RedisSock *redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU);

    if (redis_sock_disconnect(redis_sock, 1) == SUCCESS) {
        RETURN_TRUE;
    }
    RETURN_FALSE;
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::set(string key, mixed val, long timeout,
 *                              [array opt) */
PHP_METHOD(AwesomeRedis, set) {
    REDIS_PROCESS_CMD(set, redis_boolean_response);
}

/* {{{ proto boolean AwesomeRedis::setex(string key, long expire, string value)
 */
PHP_METHOD(AwesomeRedis, setex)
{
    REDIS_PROCESS_KW_CMD("SETEX", redis_key_long_val_cmd, redis_boolean_response);
}

/* {{{ proto boolean AwesomeRedis::psetex(string key, long expire, string value)
 */
PHP_METHOD(AwesomeRedis, psetex)
{
    REDIS_PROCESS_KW_CMD("PSETEX", redis_key_long_val_cmd, redis_boolean_response);
}

/* {{{ proto boolean AwesomeRedis::setnx(string key, string value)
 */
PHP_METHOD(AwesomeRedis, setnx)
{
    REDIS_PROCESS_KW_CMD("SETNX", redis_kv_cmd, redis_1_response);
}

/* }}} */

/* {{{ proto string AwesomeRedis::getSet(string key, string value)
 */
PHP_METHOD(AwesomeRedis, getSet)
{
    REDIS_PROCESS_KW_CMD("GETSET", redis_kv_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::randomKey()
 */
PHP_METHOD(AwesomeRedis, randomKey)
{
    REDIS_PROCESS_KW_CMD("RANDOMKEY", redis_empty_cmd, redis_ping_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::echo(string msg)
 */
PHP_METHOD(AwesomeRedis, echo)
{
    REDIS_PROCESS_KW_CMD("ECHO", redis_str_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::rename(string key_src, string key_dst)
 */
PHP_METHOD(AwesomeRedis, rename)
{
    REDIS_PROCESS_KW_CMD("RENAME", redis_key_key_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::renameNx(string key_src, string key_dst)
 */
PHP_METHOD(AwesomeRedis, renameNx)
{
    REDIS_PROCESS_KW_CMD("RENAMENX", redis_key_key_cmd, redis_1_response);
}
/* }}} */

/* }}} */

/* {{{ proto string AwesomeRedis::get(string key)
 */
PHP_METHOD(AwesomeRedis, get)
{
    REDIS_PROCESS_KW_CMD("GET", redis_key_cmd, redis_string_response);
}
/* }}} */


/* {{{ proto string AwesomeRedis::ping()
 */
PHP_METHOD(AwesomeRedis, ping)
{
    REDIS_PROCESS_KW_CMD("PING", redis_opt_str_cmd, redis_read_variant_reply);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::incr(string key [,int value])
 */
PHP_METHOD(AwesomeRedis, incr){
    REDIS_PROCESS_CMD(incr, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::incrBy(string key ,int value)
 */
PHP_METHOD(AwesomeRedis, incrBy){
    REDIS_PROCESS_KW_CMD("INCRBY", redis_key_long_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto float AwesomeRedis::incrByFloat(string key, float value)
 */
PHP_METHOD(AwesomeRedis, incrByFloat) {
    REDIS_PROCESS_KW_CMD("INCRBYFLOAT", redis_key_dbl_cmd,
        redis_bulk_double_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::decr(string key) */
PHP_METHOD(AwesomeRedis, decr)
{
    REDIS_PROCESS_CMD(decr, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::decrBy(string key ,int value)
 */
PHP_METHOD(AwesomeRedis, decrBy){
    REDIS_PROCESS_KW_CMD("DECRBY", redis_key_long_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::mget(array keys)
 */
PHP_METHOD(AwesomeRedis, mget)
{
    zval *object, *z_args, *z_ele;
    HashTable *hash;
    RedisSock *redis_sock;
    smart_string cmd = {0};
    int arg_count;

    /* Make sure we have proper arguments */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "Oa",
                                    &object, redis_ce, &z_args) == FAILURE) {
        RETURN_FALSE;
    }

    /* We'll need the socket */
    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    /* Grab our array */
    hash = Z_ARRVAL_P(z_args);

    /* We don't need to do anything if there aren't any keys */
    if((arg_count = zend_hash_num_elements(hash)) == 0) {
        RETURN_FALSE;
    }

    /* Build our command header */
    redis_cmd_init_sstr(&cmd, arg_count, "MGET", 4);

    /* Iterate through and grab our keys */
    ZEND_HASH_FOREACH_VAL(hash, z_ele) {
        zend_string *zstr = zval_get_string(z_ele);
        redis_cmd_append_sstr_key(&cmd, ZSTR_VAL(zstr), ZSTR_LEN(zstr), redis_sock, NULL);
        zend_string_release(zstr);
    } ZEND_HASH_FOREACH_END();

    /* Kick off our command */
    REDIS_PROCESS_REQUEST(redis_sock, cmd.c, cmd.len);
    if (IS_ATOMIC(redis_sock)) {
        if(redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                           redis_sock, NULL, NULL) < 0) {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
}

/* {{{ proto boolean AwesomeRedis::exists(string key)
 */
PHP_METHOD(AwesomeRedis, exists)
{
    REDIS_PROCESS_CMD(exists, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::del(string key)
 */
PHP_METHOD(AwesomeRedis, del)
{
    REDIS_PROCESS_CMD(del, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::unlink(string $key1, string $key2 [, string $key3...]) }}}
 * {{{ proto long AwesomeRedis::unlink(array $keys) */
PHP_METHOD(AwesomeRedis, unlink)
{
    REDIS_PROCESS_CMD(unlink, redis_long_response);
}

PHP_REDIS_API void redis_set_watch(RedisSock *redis_sock)
{
    redis_sock->watching = 1;
}

PHP_REDIS_API void redis_watch_response(INTERNAL_FUNCTION_PARAMETERS,
                                 RedisSock *redis_sock, zval *z_tab, void *ctx)
{
    redis_boolean_response_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
        z_tab, ctx, redis_set_watch);
}

/* {{{ proto boolean AwesomeRedis::watch(string key1, string key2...)
 */
PHP_METHOD(AwesomeRedis, watch)
{
    REDIS_PROCESS_CMD(watch, redis_watch_response);
}
/* }}} */

PHP_REDIS_API void redis_clear_watch(RedisSock *redis_sock)
{
    redis_sock->watching = 0;
}

PHP_REDIS_API void redis_unwatch_response(INTERNAL_FUNCTION_PARAMETERS,
                                   RedisSock *redis_sock, zval *z_tab,
                                   void *ctx)
{
    redis_boolean_response_impl(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
        z_tab, ctx, redis_clear_watch);
}

/* {{{ proto boolean AwesomeRedis::unwatch()
 */
PHP_METHOD(AwesomeRedis, unwatch)
{
    REDIS_PROCESS_KW_CMD("UNWATCH", redis_empty_cmd, redis_unwatch_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::keys(string pattern)
 */
PHP_METHOD(AwesomeRedis, keys)
{
    REDIS_PROCESS_KW_CMD("KEYS", redis_key_cmd, redis_mbulk_reply_raw);
}
/* }}} */

/* {{{ proto int AwesomeRedis::type(string key)
 */
PHP_METHOD(AwesomeRedis, type)
{
    REDIS_PROCESS_KW_CMD("TYPE", redis_key_cmd, redis_type_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::append(string key, string val) */
PHP_METHOD(AwesomeRedis, append)
{
    REDIS_PROCESS_KW_CMD("APPEND", redis_kv_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::GetRange(string key, long start, long end) */
PHP_METHOD(AwesomeRedis, getRange)
{
    REDIS_PROCESS_KW_CMD("GETRANGE", redis_key_long_long_cmd,
        redis_string_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::setRange(string key, long start, string value) */
PHP_METHOD(AwesomeRedis, setRange)
{
    REDIS_PROCESS_KW_CMD("SETRANGE", redis_key_long_str_cmd,
        redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::getbit(string key, long idx) */
PHP_METHOD(AwesomeRedis, getBit)
{
    REDIS_PROCESS_KW_CMD("GETBIT", redis_key_long_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::setbit(string key, long idx, bool|int value) */
PHP_METHOD(AwesomeRedis, setBit)
{
    REDIS_PROCESS_CMD(setbit, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::strlen(string key) */
PHP_METHOD(AwesomeRedis, strlen)
{
    REDIS_PROCESS_KW_CMD("STRLEN", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::lPush(string key , string value)
 */
PHP_METHOD(AwesomeRedis, lPush)
{
    REDIS_PROCESS_KW_CMD("LPUSH", redis_key_varval_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::rPush(string key , string value)
 */
PHP_METHOD(AwesomeRedis, rPush)
{
    REDIS_PROCESS_KW_CMD("RPUSH", redis_key_varval_cmd, redis_long_response);
}
/* }}} */

PHP_METHOD(AwesomeRedis, lInsert)
{
    REDIS_PROCESS_CMD(linsert, redis_long_response);
}

/* {{{ proto long AwesomeRedis::lPushx(string key, mixed value) */
PHP_METHOD(AwesomeRedis, lPushx)
{
    REDIS_PROCESS_KW_CMD("LPUSHX", redis_kv_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::rPushx(string key, mixed value) */
PHP_METHOD(AwesomeRedis, rPushx)
{
    REDIS_PROCESS_KW_CMD("RPUSHX", redis_kv_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::lPOP(string key) */
PHP_METHOD(AwesomeRedis, lPop)
{
    REDIS_PROCESS_KW_CMD("LPOP", redis_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::rPOP(string key) */
PHP_METHOD(AwesomeRedis, rPop)
{
    REDIS_PROCESS_KW_CMD("RPOP", redis_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::blPop(string key1, string key2, ..., int timeout) */
PHP_METHOD(AwesomeRedis, blPop)
{
    REDIS_PROCESS_KW_CMD("BLPOP", redis_blocking_pop_cmd, redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto string AwesomeRedis::brPop(string key1, string key2, ..., int timeout) */
PHP_METHOD(AwesomeRedis, brPop)
{
    REDIS_PROCESS_KW_CMD("BRPOP", redis_blocking_pop_cmd, redis_sock_read_multibulk_reply);
}
/* }}} */


/* {{{ proto int AwesomeRedis::lLen(string key) */
PHP_METHOD(AwesomeRedis, lLen)
{
    REDIS_PROCESS_KW_CMD("LLEN", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::lrem(string list, string value, int count = 0) */
PHP_METHOD(AwesomeRedis, lrem)
{
    REDIS_PROCESS_CMD(lrem, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::ltrim(string key , int start , int end) */
PHP_METHOD(AwesomeRedis, ltrim)
{
    REDIS_PROCESS_KW_CMD("LTRIM", redis_key_long_long_cmd,
        redis_boolean_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::lindex(string key , int index) */
PHP_METHOD(AwesomeRedis, lindex)
{
    REDIS_PROCESS_KW_CMD("LINDEX", redis_key_long_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::lrange(string key, int start , int end) */
PHP_METHOD(AwesomeRedis, lrange)
{
    REDIS_PROCESS_KW_CMD("LRANGE", redis_key_long_long_cmd,
        redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto long AwesomeRedis::sAdd(string key , mixed value) */
PHP_METHOD(AwesomeRedis, sAdd)
{
    REDIS_PROCESS_KW_CMD("SADD", redis_key_varval_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::sAddArray(string key, array $values) */
PHP_METHOD(AwesomeRedis, sAddArray) {
    REDIS_PROCESS_KW_CMD("SADD", redis_key_val_arr_cmd, redis_long_response);
} /* }}} */

/* {{{ proto int AwesomeRedis::scard(string key) */
PHP_METHOD(AwesomeRedis, scard)
{
    REDIS_PROCESS_KW_CMD("SCARD", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::srem(string set, string value) */
PHP_METHOD(AwesomeRedis, srem)
{
    REDIS_PROCESS_KW_CMD("SREM", redis_key_varval_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::sMove(string src, string dst, mixed value) */
PHP_METHOD(AwesomeRedis, sMove)
{
    REDIS_PROCESS_CMD(smove, redis_1_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::sPop(string key) */
PHP_METHOD(AwesomeRedis, sPop)
{
    if (ZEND_NUM_ARGS() == 1) {
        REDIS_PROCESS_KW_CMD("SPOP", redis_key_cmd, redis_string_response);
    } else if (ZEND_NUM_ARGS() == 2) {
        REDIS_PROCESS_KW_CMD("SPOP", redis_key_long_cmd, redis_sock_read_multibulk_reply);
    } else {
        ZEND_WRONG_PARAM_COUNT();
    }

}
/* }}} */

/* {{{ proto string AwesomeRedis::sRandMember(string key [int count]) */
PHP_METHOD(AwesomeRedis, sRandMember)
{
    char *cmd;
    int cmd_len;
    short have_count;
    RedisSock *redis_sock;

    // Grab our socket, validate call
    if ((redis_sock = redis_sock_get(getThis(), 0)) == NULL ||
       redis_srandmember_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
                             &cmd, &cmd_len, NULL, NULL, &have_count) == FAILURE)
    {
        RETURN_FALSE;
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if(have_count) {
        if (IS_ATOMIC(redis_sock)) {
            if(redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                               redis_sock, NULL, NULL) < 0)
            {
                RETURN_FALSE;
            }
        }
        REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
    } else {
        if (IS_ATOMIC(redis_sock)) {
            redis_string_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
                NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_string_response);
    }
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::sismember(string set, string value) */
PHP_METHOD(AwesomeRedis, sismember)
{
    REDIS_PROCESS_KW_CMD("SISMEMBER", redis_kv_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sMembers(string set) */
PHP_METHOD(AwesomeRedis, sMembers)
{
    REDIS_PROCESS_KW_CMD("SMEMBERS", redis_key_cmd,
        redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sInter(string key0, ... string keyN) */
PHP_METHOD(AwesomeRedis, sInter) {
    REDIS_PROCESS_CMD(sinter, redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sInterStore(string dst, string key0,...string keyN) */
PHP_METHOD(AwesomeRedis, sInterStore) {
    REDIS_PROCESS_CMD(sinterstore, redis_long_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sUnion(string key0, ... string keyN) */
PHP_METHOD(AwesomeRedis, sUnion) {
    REDIS_PROCESS_CMD(sunion, redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sUnionStore(string dst, string key0, ... keyN) */
PHP_METHOD(AwesomeRedis, sUnionStore) {
    REDIS_PROCESS_CMD(sunionstore, redis_long_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sDiff(string key0, ... string keyN) */
PHP_METHOD(AwesomeRedis, sDiff) {
    REDIS_PROCESS_CMD(sdiff, redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sDiffStore(string dst, string key0, ... keyN) */
PHP_METHOD(AwesomeRedis, sDiffStore) {
    REDIS_PROCESS_CMD(sdiffstore, redis_long_response);
}
/* }}} */


/* {{{ proto array AwesomeRedis::sort(string key, array options) */
PHP_METHOD(AwesomeRedis, sort) {
    char *cmd;
    int cmd_len, have_store;
    RedisSock *redis_sock;

    // Grab socket, handle command construction
    if ((redis_sock = redis_sock_get(getThis(), 0)) == NULL ||
       redis_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, &have_store,
                      &cmd, &cmd_len, NULL, NULL) == FAILURE)
    {
        RETURN_FALSE;
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if (IS_ATOMIC(redis_sock)) {
        if (redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                     redis_sock, NULL, NULL) < 0)
        {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

static void
generic_sort_cmd(INTERNAL_FUNCTION_PARAMETERS, int desc, int alpha)
{
    zval *object, *zele, *zget = NULL;
    RedisSock *redis_sock;
    zend_string *zpattern;
    char *key = NULL, *pattern = NULL, *store = NULL;
    size_t keylen, patternlen, storelen;
    zend_long offset = -1, count = -1;
    int argc = 1; /* SORT key is the simplest SORT command */
    smart_string cmd = {0};

    /* Parse myriad of sort arguments */
    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "Os|s!z!lls", &object, redis_ce, &key,
                                     &keylen, &pattern, &patternlen, &zget,
                                     &offset, &count, &store, &storelen)
                                     == FAILURE)
    {
        RETURN_FALSE;
    }

    /* Ensure we're sorting something, and we can get context */
    if (keylen == 0 || !(redis_sock = redis_sock_get(object, 0)))
        RETURN_FALSE;

    /* Start calculating argc depending on input arguments */
    if (pattern && patternlen)     argc += 2; /* BY pattern */
    if (offset >= 0 && count >= 0) argc += 3; /* LIMIT offset count */
    if (alpha)                     argc += 1; /* ALPHA */
    if (store)                     argc += 2; /* STORE destination */
    if (desc)                      argc += 1; /* DESC (ASC is the default) */

    /* GET is special.  It can be 0 .. N arguments depending what we have */
    if (zget) {
        if (Z_TYPE_P(zget) == IS_ARRAY)
            argc += zend_hash_num_elements(Z_ARRVAL_P(zget));
        else if (Z_STRLEN_P(zget) > 0) {
            argc += 2; /* GET pattern */
        }
    }

    /* Start constructing final command and append key */
    redis_cmd_init_sstr(&cmd, argc, "SORT", 4);
    redis_cmd_append_sstr_key(&cmd, key, keylen, redis_sock, NULL);

    /* BY pattern */
    if (pattern && patternlen) {
        redis_cmd_append_sstr(&cmd, "BY", sizeof("BY") - 1);
        redis_cmd_append_sstr(&cmd, pattern, patternlen);
    }

    /* LIMIT offset count */
    if (offset >= 0 && count >= 0) {
        redis_cmd_append_sstr(&cmd, "LIMIT", sizeof("LIMIT") - 1);
        redis_cmd_append_sstr_long(&cmd, offset);
        redis_cmd_append_sstr_long(&cmd, count);
    }

    /* Handle any number of GET pattern arguments we've been passed */
    if (zget != NULL) {
        if (Z_TYPE_P(zget) == IS_ARRAY) {
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(zget), zele) {
                zpattern = zval_get_string(zele);
                redis_cmd_append_sstr(&cmd, "GET", sizeof("GET") - 1);
                redis_cmd_append_sstr(&cmd, ZSTR_VAL(zpattern), ZSTR_LEN(zpattern));
                zend_string_release(zpattern);
            } ZEND_HASH_FOREACH_END();
        } else {
            zpattern = zval_get_string(zget);
            redis_cmd_append_sstr(&cmd, "GET", sizeof("GET") - 1);
            redis_cmd_append_sstr(&cmd, ZSTR_VAL(zpattern), ZSTR_LEN(zpattern));
            zend_string_release(zpattern);
        }
    }

    /* Append optional DESC and ALPHA modifiers */
    if (desc)  redis_cmd_append_sstr(&cmd, "DESC", sizeof("DESC") - 1);
    if (alpha) redis_cmd_append_sstr(&cmd, "ALPHA", sizeof("ALPHA") - 1);

    /* Finally append STORE if we've got it */
    if (store && storelen) {
        redis_cmd_append_sstr(&cmd, "STORE", sizeof("STORE") - 1);
        redis_cmd_append_sstr_key(&cmd, store, storelen, redis_sock, NULL);
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd.c, cmd.len);
    if (IS_ATOMIC(redis_sock)) {
        if (redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                     redis_sock, NULL, NULL) < 0)
        {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

/* {{{ proto array AwesomeRedis::sortAsc(string key, string pattern, string get,
 *                                int start, int end, bool getList]) */
PHP_METHOD(AwesomeRedis, sortAsc)
{
    generic_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 0);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sortAscAlpha(string key, string pattern, string get,
 *                                     int start, int end, bool getList]) */
PHP_METHOD(AwesomeRedis, sortAscAlpha)
{
    generic_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0, 1);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sortDesc(string key, string pattern, string get,
 *                                 int start, int end, bool getList]) */
PHP_METHOD(AwesomeRedis, sortDesc)
{
    generic_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 0);
}
/* }}} */

/* {{{ proto array AwesomeRedis::sortDescAlpha(string key, string pattern, string get,
 *                                      int start, int end, bool getList]) */
PHP_METHOD(AwesomeRedis, sortDescAlpha)
{
    generic_sort_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1, 1);
}
/* }}} */

/* {{{ proto array AwesomeRedis::expire(string key, int timeout) */
PHP_METHOD(AwesomeRedis, expire) {
    REDIS_PROCESS_KW_CMD("EXPIRE", redis_key_long_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto bool AwesomeRedis::pexpire(string key, long ms) */
PHP_METHOD(AwesomeRedis, pexpire) {
    REDIS_PROCESS_KW_CMD("PEXPIRE", redis_key_long_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::expireAt(string key, int timestamp) */
PHP_METHOD(AwesomeRedis, expireAt) {
    REDIS_PROCESS_KW_CMD("EXPIREAT", redis_key_long_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::pexpireAt(string key, int timestamp) */
PHP_METHOD(AwesomeRedis, pexpireAt) {
    REDIS_PROCESS_KW_CMD("PEXPIREAT", redis_key_long_cmd, redis_1_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::lSet(string key, int index, string value) */
PHP_METHOD(AwesomeRedis, lSet) {
    REDIS_PROCESS_KW_CMD("LSET", redis_key_long_val_cmd,
        redis_boolean_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::save() */
PHP_METHOD(AwesomeRedis, save)
{
    REDIS_PROCESS_KW_CMD("SAVE", redis_empty_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::bgSave() */
PHP_METHOD(AwesomeRedis, bgSave)
{
    REDIS_PROCESS_KW_CMD("BGSAVE", redis_empty_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto integer AwesomeRedis::lastSave() */
PHP_METHOD(AwesomeRedis, lastSave)
{
    REDIS_PROCESS_KW_CMD("LASTSAVE", redis_empty_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto bool AwesomeRedis::flushDB([bool async]) */
PHP_METHOD(AwesomeRedis, flushDB)
{
    REDIS_PROCESS_KW_CMD("FLUSHDB", redis_flush_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto bool AwesomeRedis::flushAll([bool async]) */
PHP_METHOD(AwesomeRedis, flushAll)
{
    REDIS_PROCESS_KW_CMD("FLUSHALL", redis_flush_cmd, redis_boolean_response);
}
/* }}} */

/* {{{ proto int AwesomeRedis::dbSize() */
PHP_METHOD(AwesomeRedis, dbSize)
{
    REDIS_PROCESS_KW_CMD("DBSIZE", redis_empty_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto bool AwesomeRedis::auth(string passwd) */
PHP_METHOD(AwesomeRedis, auth) {
    REDIS_PROCESS_CMD(auth, redis_boolean_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::persist(string key) */
PHP_METHOD(AwesomeRedis, persist) {
    REDIS_PROCESS_KW_CMD("PERSIST", redis_key_cmd, redis_1_response);
}
/* }}} */


/* {{{ proto long AwesomeRedis::ttl(string key) */
PHP_METHOD(AwesomeRedis, ttl) {
    REDIS_PROCESS_KW_CMD("TTL", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::pttl(string key) */
PHP_METHOD(AwesomeRedis, pttl) {
    REDIS_PROCESS_KW_CMD("PTTL", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::info() */
PHP_METHOD(AwesomeRedis, info) {

    zval *object;
    RedisSock *redis_sock;
    char *cmd, *opt = NULL;
    size_t opt_len;
    int cmd_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "O|s", &object, redis_ce, &opt, &opt_len)
                                     == FAILURE)
    {
        RETURN_FALSE;
    }

    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    /* Build a standalone INFO command or one with an option */
    if (opt != NULL) {
        cmd_len = REDIS_SPPRINTF(&cmd, "INFO", "s", opt, opt_len);
    } else {
        cmd_len = REDIS_SPPRINTF(&cmd, "INFO", "");
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if (IS_ATOMIC(redis_sock)) {
        redis_info_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL,
            NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_info_response);

}
/* }}} */

/* {{{ proto bool AwesomeRedis::select(long dbNumber) */
PHP_METHOD(AwesomeRedis, select) {

    zval *object;
    RedisSock *redis_sock;

    char *cmd;
    int cmd_len;
    zend_long dbNumber;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "Ol",
                                     &object, redis_ce, &dbNumber) == FAILURE) {
        RETURN_FALSE;
    }

    if (dbNumber < 0 || (redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    redis_sock->dbNumber = dbNumber;
    cmd_len = REDIS_SPPRINTF(&cmd, "SELECT", "d", dbNumber);

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if (IS_ATOMIC(redis_sock)) {
        redis_boolean_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
            NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_boolean_response);
}
/* }}} */

/* {{{ proto bool AwesomeRedis::swapdb(long srcdb, long dstdb) */
PHP_METHOD(AwesomeRedis, swapdb) {
    REDIS_PROCESS_KW_CMD("SWAPDB", redis_long_long_cmd, redis_boolean_response);
}

/* {{{ proto bool AwesomeRedis::move(string key, long dbindex) */
PHP_METHOD(AwesomeRedis, move) {
    REDIS_PROCESS_KW_CMD("MOVE", redis_key_long_cmd, redis_1_response);
}
/* }}} */

static
void generic_mset(INTERNAL_FUNCTION_PARAMETERS, char *kw, ResultCallback fun)
{
    RedisSock *redis_sock;
    smart_string cmd = {0};
    zval *object, *z_array;
    HashTable *htargs;
    zend_string *zkey;
    zval *zmem;
    char buf[64];
    size_t keylen;
    zend_ulong idx;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "Oa",
                                     &object, redis_ce, &z_array) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* Make sure we can get our socket, and we were not passed an empty array */
    if ((redis_sock = redis_sock_get(object, 0)) == NULL ||
        zend_hash_num_elements(Z_ARRVAL_P(z_array)) == 0)
    {
        RETURN_FALSE;
    }

    /* Initialize our command */
    htargs = Z_ARRVAL_P(z_array);
    redis_cmd_init_sstr(&cmd, zend_hash_num_elements(htargs) * 2, kw, strlen(kw));

    ZEND_HASH_FOREACH_KEY_VAL(htargs, idx, zkey,  zmem) {
        /* Handle string or numeric keys */
        if (zkey) {
            redis_cmd_append_sstr_key(&cmd, ZSTR_VAL(zkey), ZSTR_LEN(zkey), redis_sock, NULL);
        } else {
            keylen = snprintf(buf, sizeof(buf), "%ld", (long)idx);
            redis_cmd_append_sstr_key(&cmd, buf, keylen, redis_sock, NULL);
        }

        /* Append our value */
        redis_cmd_append_sstr_zval(&cmd, zmem, redis_sock);
    } ZEND_HASH_FOREACH_END();

    REDIS_PROCESS_REQUEST(redis_sock, cmd.c, cmd.len);
    if (IS_ATOMIC(redis_sock)) {
        fun(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(fun);
}

/* {{{ proto bool AwesomeRedis::mset(array (key => value, ...)) */
PHP_METHOD(AwesomeRedis, mset) {
    generic_mset(INTERNAL_FUNCTION_PARAM_PASSTHRU, "MSET", redis_boolean_response);
}
/* }}} */


/* {{{ proto bool AwesomeRedis::msetnx(array (key => value, ...)) */
PHP_METHOD(AwesomeRedis, msetnx) {
    generic_mset(INTERNAL_FUNCTION_PARAM_PASSTHRU, "MSETNX", redis_1_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::rpoplpush(string srckey, string dstkey) */
PHP_METHOD(AwesomeRedis, rpoplpush)
{
    REDIS_PROCESS_KW_CMD("RPOPLPUSH", redis_key_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::brpoplpush(string src, string dst, int timeout) */
PHP_METHOD(AwesomeRedis, brpoplpush) {
    REDIS_PROCESS_CMD(brpoplpush, redis_string_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zAdd(string key, int score, string value) */
PHP_METHOD(AwesomeRedis, zAdd) {
    REDIS_PROCESS_CMD(zadd, redis_long_response);
}
/* }}} */

/* Handle ZRANGE and ZREVRANGE as they're the same except for keyword */
static void generic_zrange_cmd(INTERNAL_FUNCTION_PARAMETERS, char *kw,
                               zrange_cb fun)
{
    char *cmd;
    int cmd_len;
    RedisSock *redis_sock;
    int withscores = 0;

    if ((redis_sock = redis_sock_get(getThis(), 0)) == NULL) {
        RETURN_FALSE;
    }

    if(fun(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, kw, &cmd,
           &cmd_len, &withscores, NULL, NULL) == FAILURE)
    {
        RETURN_FALSE;
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if(withscores) {
        if (IS_ATOMIC(redis_sock)) {
            redis_mbulk_reply_zipped_keys_dbl(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_mbulk_reply_zipped_keys_dbl);
    } else {
        if (IS_ATOMIC(redis_sock)) {
            if(redis_sock_read_multibulk_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                               redis_sock, NULL, NULL) < 0)
            {
                RETURN_FALSE;
            }
        }
        REDIS_PROCESS_RESPONSE(redis_sock_read_multibulk_reply);
    }
}

/* {{{ proto array AwesomeRedis::zRange(string key,int start,int end,bool scores = 0) */
PHP_METHOD(AwesomeRedis, zRange)
{
    generic_zrange_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZRANGE",
        redis_zrange_cmd);
}

/* {{{ proto array AwesomeRedis::zRevRange(string k, long s, long e, bool scores = 0) */
PHP_METHOD(AwesomeRedis, zRevRange) {
    generic_zrange_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZREVRANGE",
        redis_zrange_cmd);
}
/* }}} */

/* {{{ proto array AwesomeRedis::zRangeByScore(string k,string s,string e,array opt) */
PHP_METHOD(AwesomeRedis, zRangeByScore) {
    generic_zrange_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZRANGEBYSCORE",
        redis_zrangebyscore_cmd);
}
/* }}} */

/* {{{ proto array AwesomeRedis::zRevRangeByScore(string key, string start, string end,
 *                                         array options) */
PHP_METHOD(AwesomeRedis, zRevRangeByScore) {
    generic_zrange_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, "ZREVRANGEBYSCORE",
        redis_zrangebyscore_cmd);
}
/* }}} */

/* {{{ proto array AwesomeRedis::zRangeByLex(string key, string min, string max, [
 *                                    offset, limit]) */
PHP_METHOD(AwesomeRedis, zRangeByLex) {
    REDIS_PROCESS_KW_CMD("ZRANGEBYLEX", redis_zrangebylex_cmd,
        redis_sock_read_multibulk_reply);
}
/* }}} */

PHP_METHOD(AwesomeRedis, zRevRangeByLex) {
    REDIS_PROCESS_KW_CMD("ZREVRANGEBYLEX", redis_zrangebylex_cmd,
        redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zLexCount(string key, string min, string max) */
PHP_METHOD(AwesomeRedis, zLexCount) {
    REDIS_PROCESS_KW_CMD("ZLEXCOUNT", redis_gen_zlex_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zRemRangeByLex(string key, string min, string max) */
PHP_METHOD(AwesomeRedis, zRemRangeByLex) {
    REDIS_PROCESS_KW_CMD("ZREMRANGEBYLEX", redis_gen_zlex_cmd,
        redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zRem(string key, string member) */
PHP_METHOD(AwesomeRedis, zRem)
{
    REDIS_PROCESS_KW_CMD("ZREM", redis_key_varval_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zRemRangeByScore(string k, string s, string e) */
PHP_METHOD(AwesomeRedis, zRemRangeByScore)
{
    REDIS_PROCESS_KW_CMD("ZREMRANGEBYSCORE", redis_key_str_str_cmd,
        redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zRemRangeByRank(string key, long start, long end) */
PHP_METHOD(AwesomeRedis, zRemRangeByRank)
{
    REDIS_PROCESS_KW_CMD("ZREMRANGEBYRANK", redis_key_long_long_cmd,
        redis_long_response);
}
/* }}} */

/* {{{ proto array AwesomeRedis::zCount(string key, string start , string end) */
PHP_METHOD(AwesomeRedis, zCount)
{
    REDIS_PROCESS_KW_CMD("ZCOUNT", redis_key_str_str_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zCard(string key) */
PHP_METHOD(AwesomeRedis, zCard)
{
    REDIS_PROCESS_KW_CMD("ZCARD", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto double AwesomeRedis::zScore(string key, mixed member) */
PHP_METHOD(AwesomeRedis, zScore)
{
    REDIS_PROCESS_KW_CMD("ZSCORE", redis_kv_cmd,
        redis_bulk_double_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zRank(string key, string member) */
PHP_METHOD(AwesomeRedis, zRank) {
    REDIS_PROCESS_KW_CMD("ZRANK", redis_kv_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::zRevRank(string key, string member) */
PHP_METHOD(AwesomeRedis, zRevRank) {
    REDIS_PROCESS_KW_CMD("ZREVRANK", redis_kv_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto double AwesomeRedis::zIncrBy(string key, double value, mixed member) */
PHP_METHOD(AwesomeRedis, zIncrBy)
{
    REDIS_PROCESS_CMD(zincrby, redis_bulk_double_response);
}
/* }}} */

/* zinterstore */
PHP_METHOD(AwesomeRedis, zinterstore) {
    REDIS_PROCESS_KW_CMD("ZINTERSTORE", redis_zinter_cmd, redis_long_response);
}

/* zunionstore */
PHP_METHOD(AwesomeRedis, zunionstore) {
    REDIS_PROCESS_KW_CMD("ZUNIONSTORE", redis_zinter_cmd, redis_long_response);
}

/* {{{ proto array AwesomeRedis::zPopMax(string key) */
PHP_METHOD(AwesomeRedis, zPopMax)
{
    if (ZEND_NUM_ARGS() == 1) {
        REDIS_PROCESS_KW_CMD("ZPOPMAX", redis_key_cmd, redis_mbulk_reply_zipped_keys_dbl);
    } else if (ZEND_NUM_ARGS() == 2) {
        REDIS_PROCESS_KW_CMD("ZPOPMAX", redis_key_long_cmd, redis_mbulk_reply_zipped_keys_dbl);
    } else {
        ZEND_WRONG_PARAM_COUNT();
    }
}
/* }}} */

/* {{{ proto array AwesomeRedis::zPopMin(string key) */
PHP_METHOD(AwesomeRedis, zPopMin)
{
    if (ZEND_NUM_ARGS() == 1) {
        REDIS_PROCESS_KW_CMD("ZPOPMIN", redis_key_cmd, redis_mbulk_reply_zipped_keys_dbl);
    } else if (ZEND_NUM_ARGS() == 2) {
        REDIS_PROCESS_KW_CMD("ZPOPMIN", redis_key_long_cmd, redis_mbulk_reply_zipped_keys_dbl);
    } else {
        ZEND_WRONG_PARAM_COUNT();
    }
}
/* }}} */

/* {{{ proto AwesomeRedis::bzPopMax(Array(keys) [, timeout]): Array */
PHP_METHOD(AwesomeRedis, bzPopMax) {
    REDIS_PROCESS_KW_CMD("BZPOPMAX", redis_blocking_pop_cmd, redis_sock_read_multibulk_reply);
}
/* }}} */

/* {{{ proto AwesomeRedis::bzPopMin(Array(keys) [, timeout]): Array */
PHP_METHOD(AwesomeRedis, bzPopMin) {
    REDIS_PROCESS_KW_CMD("BZPOPMIN", redis_blocking_pop_cmd, redis_sock_read_multibulk_reply);
}
/* }}} */

/* hashes */

/* {{{ proto long AwesomeRedis::hset(string key, string mem, string val) */
PHP_METHOD(AwesomeRedis, hSet)
{
    REDIS_PROCESS_CMD(hset, redis_long_response);
}
/* }}} */

/* {{{ proto bool AwesomeRedis::hSetNx(string key, string mem, string val) */
PHP_METHOD(AwesomeRedis, hSetNx)
{
    REDIS_PROCESS_CMD(hsetnx, redis_1_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::hget(string key, string mem) */
PHP_METHOD(AwesomeRedis, hGet)
{
    REDIS_PROCESS_KW_CMD("HGET", redis_key_str_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::hLen(string key) */
PHP_METHOD(AwesomeRedis, hLen)
{
    REDIS_PROCESS_KW_CMD("HLEN", redis_key_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::hDel(string key, string mem1, ... memN) */
PHP_METHOD(AwesomeRedis, hDel)
{
    REDIS_PROCESS_CMD(hdel, redis_long_response);
}
/* }}} */

/* {{{ proto bool AwesomeRedis::hExists(string key, string mem) */
PHP_METHOD(AwesomeRedis, hExists)
{
    REDIS_PROCESS_KW_CMD("HEXISTS", redis_key_str_cmd, redis_1_response);
}

/* {{{ proto array AwesomeRedis::hkeys(string key) */
PHP_METHOD(AwesomeRedis, hKeys)
{
    REDIS_PROCESS_KW_CMD("HKEYS", redis_key_cmd, redis_mbulk_reply_raw);
}
/* }}} */

/* {{{ proto array AwesomeRedis::hvals(string key) */
PHP_METHOD(AwesomeRedis, hVals)
{
    REDIS_PROCESS_KW_CMD("HVALS", redis_key_cmd,
        redis_sock_read_multibulk_reply);
}

/* {{{ proto array AwesomeRedis::hgetall(string key) */
PHP_METHOD(AwesomeRedis, hGetAll) {
    REDIS_PROCESS_KW_CMD("HGETALL", redis_key_cmd, redis_mbulk_reply_zipped_vals);
}
/* }}} */

/* {{{ proto double AwesomeRedis::hIncrByFloat(string k, string me, double v) */
PHP_METHOD(AwesomeRedis, hIncrByFloat)
{
    REDIS_PROCESS_CMD(hincrbyfloat, redis_bulk_double_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::hincrby(string key, string mem, long byval) */
PHP_METHOD(AwesomeRedis, hIncrBy)
{
    REDIS_PROCESS_CMD(hincrby, redis_long_response);
}
/* }}} */

/* {{{ array AwesomeRedis::hMget(string hash, array keys) */
PHP_METHOD(AwesomeRedis, hMget) {
    REDIS_PROCESS_CMD(hmget, redis_mbulk_reply_assoc);
}
/* }}} */

/* {{{ proto bool AwesomeRedis::hmset(string key, array keyvals) */
PHP_METHOD(AwesomeRedis, hMset)
{
    REDIS_PROCESS_CMD(hmset, redis_boolean_response);
}
/* }}} */

/* {{{ proto long AwesomeRedis::hstrlen(string key, string field) */
PHP_METHOD(AwesomeRedis, hStrLen) {
    REDIS_PROCESS_CMD(hstrlen, redis_long_response);
}
/* }}} */

/* flag : get, set {ATOMIC, MULTI, PIPELINE} */

PHP_METHOD(AwesomeRedis, multi)
{

    RedisSock *redis_sock;
    char *resp, *cmd;
    int resp_len, cmd_len;
    zval *object;
    zend_long multi_value = MULTI;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "O|l", &object, redis_ce, &multi_value)
                                     == FAILURE)
    {
        RETURN_FALSE;
    }

    /* if the flag is activated, send the command, the reply will be "QUEUED"
     * or -ERR */
    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    if (multi_value == PIPELINE) {
        /* Cannot enter pipeline mode in a MULTI block */
        if (IS_MULTI(redis_sock)) {
            php_error_docref(NULL, E_ERROR, "Can't activate pipeline in multi mode!");
            RETURN_FALSE;
        }

        /* Enable PIPELINE if we're not already in one */
        if (IS_ATOMIC(redis_sock)) {
            free_reply_callbacks(redis_sock);
            REDIS_ENABLE_MODE(redis_sock, PIPELINE);
        }
    } else if (multi_value == MULTI) {
        /* Don't want to do anything if we're alredy in MULTI mode */
        if (!IS_MULTI(redis_sock)) {
            cmd_len = REDIS_SPPRINTF(&cmd, "MULTI", "");
            if (IS_PIPELINE(redis_sock)) {
                PIPELINE_ENQUEUE_COMMAND(cmd, cmd_len);
                efree(cmd);
                REDIS_SAVE_CALLBACK(NULL, NULL);
                REDIS_ENABLE_MODE(redis_sock, MULTI);
            } else {
                SOCKET_WRITE_COMMAND(redis_sock, cmd, cmd_len)
                efree(cmd);
                if ((resp = redis_sock_read(redis_sock, &resp_len)) == NULL) {
                    RETURN_FALSE;
                } else if (strncmp(resp, "+OK", 3) != 0) {
                    efree(resp);
                    RETURN_FALSE;
                }
                efree(resp);
                REDIS_ENABLE_MODE(redis_sock, MULTI);
            }
        }
    } else {
        php_error_docref(NULL, E_WARNING, "Unknown mode sent to AwesomeRedis::multi");
        RETURN_FALSE;
    }

    RETURN_ZVAL(getThis(), 1, 0);
}

/* discard */
PHP_METHOD(AwesomeRedis, discard)
{
    int ret = FAILURE;
    RedisSock *redis_sock;
    zval *object;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "O",
                                     &object, redis_ce) == FAILURE) {
        RETURN_FALSE;
    }

    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    if (IS_PIPELINE(redis_sock)) {
        ret = SUCCESS;
        if (redis_sock->pipeline_cmd) {
            zend_string_release(redis_sock->pipeline_cmd);
            redis_sock->pipeline_cmd = NULL;
        }
    } else if (IS_MULTI(redis_sock)) {
        ret = redis_send_discard(redis_sock);
    }
    if (ret == SUCCESS) {
        free_reply_callbacks(redis_sock);
        redis_sock->mode = ATOMIC;
        RETURN_TRUE;
    }
    RETURN_FALSE;
}

/* redis_sock_read_multibulk_multi_reply */
PHP_REDIS_API int redis_sock_read_multibulk_multi_reply(INTERNAL_FUNCTION_PARAMETERS,
                                      RedisSock *redis_sock)
{

    char inbuf[4096];
    int numElems;
    size_t len;

    if (redis_sock_gets(redis_sock, inbuf, sizeof(inbuf) - 1, &len) < 0) {
        return - 1;
    }

    /* number of responses */
    numElems = atoi(inbuf+1);

    if(numElems < 0) {
        return -1;
    }

    array_init(return_value);

    redis_sock_read_multibulk_multi_reply_loop(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                    redis_sock, return_value, numElems);

    return 0;
}


/* exec */
PHP_METHOD(AwesomeRedis, exec)
{
    RedisSock *redis_sock;
    char *cmd;
    int cmd_len, ret;
    zval *object;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "O", &object, redis_ce) == FAILURE ||
        (redis_sock = redis_sock_get(object, 0)) == NULL
    ) {
        RETURN_FALSE;
    }

    if (IS_MULTI(redis_sock)) {
        cmd_len = REDIS_SPPRINTF(&cmd, "EXEC", "");
        if (IS_PIPELINE(redis_sock)) {
            PIPELINE_ENQUEUE_COMMAND(cmd, cmd_len);
            efree(cmd);
            REDIS_SAVE_CALLBACK(NULL, NULL);
            REDIS_DISABLE_MODE(redis_sock, MULTI);
            RETURN_ZVAL(getThis(), 1, 0);
        }
        SOCKET_WRITE_COMMAND(redis_sock, cmd, cmd_len)
        efree(cmd);

        ret = redis_sock_read_multibulk_multi_reply(
            INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock);
        free_reply_callbacks(redis_sock);
        REDIS_DISABLE_MODE(redis_sock, MULTI);
        redis_sock->watching = 0;
        if (ret < 0) {
            zval_dtor(return_value);
            RETURN_FALSE;
        }
    }

    if (IS_PIPELINE(redis_sock)) {
        if (redis_sock->pipeline_cmd == NULL) {
            /* Empty array when no command was run. */
            array_init(return_value);
        } else {
            if (redis_sock_write(redis_sock, ZSTR_VAL(redis_sock->pipeline_cmd),
                    ZSTR_LEN(redis_sock->pipeline_cmd)) < 0) {
                ZVAL_FALSE(return_value);
            } else {
                array_init(return_value);
                redis_sock_read_multibulk_multi_reply_loop(
                    INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, return_value, 0);
            }
            zend_string_release(redis_sock->pipeline_cmd);
            redis_sock->pipeline_cmd = NULL;
        }
        free_reply_callbacks(redis_sock);
        REDIS_DISABLE_MODE(redis_sock, PIPELINE);
    }
}

PHP_REDIS_API int
redis_response_enqueued(RedisSock *redis_sock)
{
    char *resp;
    int resp_len, ret = FAILURE;

    if ((resp = redis_sock_read(redis_sock, &resp_len)) != NULL) {
        if (strncmp(resp, "+QUEUED", 7) == 0) {
            ret = SUCCESS;
        }
        efree(resp);
    }
    return ret;
}

/* TODO:  Investigate/fix the odd logic going on in here.  Looks like previous abort
 *        condidtions that are now simply empty if { } { } blocks. */
PHP_REDIS_API int
redis_sock_read_multibulk_multi_reply_loop(INTERNAL_FUNCTION_PARAMETERS,
                                           RedisSock *redis_sock, zval *z_tab,
                                           int numElems)
{
    fold_item *fi;

    for (fi = redis_sock->head; fi; /* void */) {
        if (fi->fun) {
            fi->fun(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, z_tab,
                fi->ctx);
            fi = fi->next;
            continue;
        }
        size_t len;
        char inbuf[255];

        if (redis_sock_gets(redis_sock, inbuf, sizeof(inbuf) - 1, &len) < 0) {
        } else if (strncmp(inbuf, "+OK", 3) != 0) {
        }

        while ((fi = fi->next) && fi->fun) {
            if (redis_response_enqueued(redis_sock) == SUCCESS) {
            } else {
            }
        }

        if (redis_sock_gets(redis_sock, inbuf, sizeof(inbuf) - 1, &len) < 0) {
        }

        zval z_ret;
        array_init(&z_ret);
        add_next_index_zval(z_tab, &z_ret);

        int num = atol(inbuf + 1);

        if (num > 0 && redis_read_multibulk_recursive(redis_sock, num, 0, &z_ret) < 0) {
        }

        if (fi) fi = fi->next;
    }
    redis_sock->current = fi;
    return 0;
}

PHP_METHOD(AwesomeRedis, pipeline)
{
    RedisSock *redis_sock;
    zval *object;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "O", &object, redis_ce) == FAILURE ||
        (redis_sock = redis_sock_get(object, 0)) == NULL
    ) {
        RETURN_FALSE;
    }

    /* User cannot enter MULTI mode if already in a pipeline */
    if (IS_MULTI(redis_sock)) {
        php_error_docref(NULL, E_ERROR, "Can't activate pipeline in multi mode!");
        RETURN_FALSE;
    }

    /* Enable pipeline mode unless we're already in that mode in which case this
     * is just a NO OP */
    if (IS_ATOMIC(redis_sock)) {
        /* NB : we keep the function fold, to detect the last function.
         * We need the response format of the n - 1 command. So, we can delete
         * when n > 2, the { 1 .. n - 2} commands */
        free_reply_callbacks(redis_sock);
        REDIS_ENABLE_MODE(redis_sock, PIPELINE);
    }

    RETURN_ZVAL(getThis(), 1, 0);
}

/* {{{ proto long AwesomeRedis::publish(string channel, string msg) */
PHP_METHOD(AwesomeRedis, publish)
{
    REDIS_PROCESS_KW_CMD("PUBLISH", redis_key_str_cmd, redis_long_response);
}
/* }}} */

/* {{{ proto void AwesomeRedis::psubscribe(Array(pattern1, pattern2, ... patternN)) */
PHP_METHOD(AwesomeRedis, psubscribe)
{
    REDIS_PROCESS_KW_CMD("PSUBSCRIBE", redis_subscribe_cmd,
        redis_subscribe_response);
}

/* {{{ proto void AwesomeRedis::subscribe(Array(channel1, channel2, ... channelN)) */
PHP_METHOD(AwesomeRedis, subscribe) {
    REDIS_PROCESS_KW_CMD("SUBSCRIBE", redis_subscribe_cmd,
        redis_subscribe_response);
}

/**
 *  [p]unsubscribe channel_0 channel_1 ... channel_n
 *  [p]unsubscribe(array(channel_0, channel_1, ..., channel_n))
 * response format :
 * array(
 *    channel_0 => TRUE|FALSE,
 *    channel_1 => TRUE|FALSE,
 *    ...
 *    channel_n => TRUE|FALSE
 * );
 **/

PHP_REDIS_API void generic_unsubscribe_cmd(INTERNAL_FUNCTION_PARAMETERS,
                                    char *unsub_cmd)
{
    zval *object, *array, *data;
    HashTable *arr_hash;
    RedisSock *redis_sock;
    char *cmd = "", *old_cmd = NULL;
    int cmd_len, array_count;

    int i;
    zval z_tab, *z_channel;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "Oa",
                                     &object, redis_ce, &array) == FAILURE) {
        RETURN_FALSE;
    }
    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    arr_hash    = Z_ARRVAL_P(array);
    array_count = zend_hash_num_elements(arr_hash);

    if (array_count == 0) {
        RETURN_FALSE;
    }

    ZEND_HASH_FOREACH_VAL(arr_hash, data) {
        ZVAL_DEREF(data);
        if (Z_TYPE_P(data) == IS_STRING) {
            char *old_cmd = NULL;
            if(*cmd) {
                old_cmd = cmd;
            }
            spprintf(&cmd, 0, "%s %s", cmd, Z_STRVAL_P(data));
            if(old_cmd) {
                efree(old_cmd);
            }
        }
    } ZEND_HASH_FOREACH_END();

    old_cmd = cmd;
    cmd_len = spprintf(&cmd, 0, "%s %s\r\n", unsub_cmd, cmd);
    efree(old_cmd);

    SOCKET_WRITE_COMMAND(redis_sock, cmd, cmd_len)
    efree(cmd);

    array_init(return_value);
    for (i = 1; i <= array_count; i++) {
        redis_sock_read_multibulk_reply_zval(
            INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, &z_tab);

        if (Z_TYPE(z_tab) == IS_ARRAY) {
            if ((z_channel = zend_hash_index_find(Z_ARRVAL(z_tab), 1)) == NULL) {
                RETURN_FALSE;
            }
            add_assoc_bool(return_value, Z_STRVAL_P(z_channel), 1);
        } else {
            //error
            zval_dtor(&z_tab);
            RETURN_FALSE;
        }
        zval_dtor(&z_tab);
    }
}

PHP_METHOD(AwesomeRedis, unsubscribe)
{
    REDIS_PROCESS_KW_CMD("UNSUBSCRIBE", redis_unsubscribe_cmd,
        redis_unsubscribe_response);
}

PHP_METHOD(AwesomeRedis, punsubscribe)
{
    REDIS_PROCESS_KW_CMD("PUNSUBSCRIBE", redis_unsubscribe_cmd,
        redis_unsubscribe_response);
}

/* {{{ proto string AwesomeRedis::bgrewriteaof() */
PHP_METHOD(AwesomeRedis, bgrewriteaof)
{
    REDIS_PROCESS_KW_CMD("BGREWRITEAOF", redis_empty_cmd,
        redis_boolean_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::slaveof([host, port]) */
PHP_METHOD(AwesomeRedis, slaveof)
{
    zval *object;
    RedisSock *redis_sock;
    char *cmd = "", *host = NULL;
    size_t host_len;
    zend_long port = 6379;
    int cmd_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "O|sl", &object, redis_ce, &host,
                                     &host_len, &port) == FAILURE)
    {
        RETURN_FALSE;
    }
    if (port < 0 || (redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    if (host && host_len) {
        cmd_len = REDIS_SPPRINTF(&cmd, "SLAVEOF", "sd", host, host_len, (int)port);
    } else {
        cmd_len = REDIS_SPPRINTF(&cmd, "SLAVEOF", "ss", "NO", 2, "ONE", 3);
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if (IS_ATOMIC(redis_sock)) {
      redis_boolean_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
          NULL, NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_boolean_response);
}
/* }}} */

/* {{{ proto string AwesomeRedis::object(key) */
PHP_METHOD(AwesomeRedis, object)
{
    RedisSock *redis_sock;
    char *cmd; int cmd_len;
    REDIS_REPLY_TYPE rtype;

    if ((redis_sock = redis_sock_get(getThis(), 0)) == NULL) {
       RETURN_FALSE;
    }

    if(redis_object_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, &rtype,
                        &cmd, &cmd_len, NULL, NULL)==FAILURE)
    {
       RETURN_FALSE;
    }

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);

    if(rtype == TYPE_INT) {
        if (IS_ATOMIC(redis_sock)) {
            redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
                NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_long_response);
    } else {
        if (IS_ATOMIC(redis_sock)) {
            redis_string_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
                NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_string_response);
    }
}
/* }}} */

/* {{{ proto string AwesomeRedis::getOption($option) */
PHP_METHOD(AwesomeRedis, getOption)
{
    RedisSock *redis_sock;

    if ((redis_sock = redis_sock_get_instance(getThis(), 0)) == NULL) {
        RETURN_FALSE;
    }

    redis_getoption_handler(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL);
}
/* }}} */

/* {{{ proto string AwesomeRedis::setOption(string $option, mixed $value) */
PHP_METHOD(AwesomeRedis, setOption)
{
    RedisSock *redis_sock;

    if ((redis_sock = redis_sock_get_instance(getThis(), 0)) == NULL) {
        RETURN_FALSE;
    }

    redis_setoption_handler(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL);
}
/* }}} */

/* {{{ proto boolean AwesomeRedis::config(string op, string key [, mixed value]) */
PHP_METHOD(AwesomeRedis, config)
{
    zval *object;
    RedisSock *redis_sock;
    char *key = NULL, *val = NULL, *cmd, *op = NULL;
    size_t key_len, val_len, op_len;
    enum {CFG_GET, CFG_SET} mode;
    int cmd_len;

    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                     "Oss|s", &object, redis_ce, &op, &op_len,
                                     &key, &key_len, &val, &val_len) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* op must be GET or SET */
    if(strncasecmp(op, "GET", 3) == 0) {
        mode = CFG_GET;
    } else if(strncasecmp(op, "SET", 3) == 0) {
        mode = CFG_SET;
    } else {
        RETURN_FALSE;
    }

    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    if (mode == CFG_GET && val == NULL) {
        cmd_len = REDIS_SPPRINTF(&cmd, "CONFIG", "ss", op, op_len, key, key_len);

        REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len)
        if (IS_ATOMIC(redis_sock)) {
            redis_mbulk_reply_zipped_raw(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_mbulk_reply_zipped_raw);

    } else if(mode == CFG_SET && val != NULL) {
        cmd_len = REDIS_SPPRINTF(&cmd, "CONFIG", "sss", op, op_len, key, key_len, val, val_len);

        REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len)
        if (IS_ATOMIC(redis_sock)) {
            redis_boolean_response(
                INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL, NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_boolean_response);
    } else {
        RETURN_FALSE;
    }
}
/* }}} */


/* {{{ proto boolean AwesomeRedis::slowlog(string arg, [int option]) */
PHP_METHOD(AwesomeRedis, slowlog) {
    zval *object;
    RedisSock *redis_sock;
    char *arg, *cmd;
    int cmd_len;
    size_t arg_len;
    zend_long option = 0;
    enum {SLOWLOG_GET, SLOWLOG_LEN, SLOWLOG_RESET} mode;

    // Make sure we can get parameters
    if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                    "Os|l", &object, redis_ce, &arg, &arg_len,
                                    &option) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* Figure out what kind of slowlog command we're executing */
    if(!strncasecmp(arg, "GET", 3)) {
        mode = SLOWLOG_GET;
    } else if(!strncasecmp(arg, "LEN", 3)) {
        mode = SLOWLOG_LEN;
    } else if(!strncasecmp(arg, "RESET", 5)) {
        mode = SLOWLOG_RESET;
    } else {
        /* This command is not valid */
        RETURN_FALSE;
    }

    /* Make sure we can grab our redis socket */
    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    // Create our command.  For everything except SLOWLOG GET (with an arg) it's
    // just two parts
    if (mode == SLOWLOG_GET && ZEND_NUM_ARGS() == 2) {
        cmd_len = REDIS_SPPRINTF(&cmd, "SLOWLOG", "sl", arg, arg_len, option);
    } else {
        cmd_len = REDIS_SPPRINTF(&cmd, "SLOWLOG", "s", arg, arg_len);
    }

    /* Kick off our command */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if (IS_ATOMIC(redis_sock)) {
        if(redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                    redis_sock, NULL, NULL) < 0)
        {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

/* {{{ proto AwesomeRedis::wait(int num_slaves, int ms) }}} */
PHP_METHOD(AwesomeRedis, wait) {
    zval *object;
    RedisSock *redis_sock;
    zend_long num_slaves, timeout;
    char *cmd;
    int cmd_len;

    /* Make sure arguments are valid */
    if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "Oll",
                                    &object, redis_ce, &num_slaves, &timeout)
                                    ==FAILURE)
    {
        RETURN_FALSE;
    }

    /* Don't even send this to Redis if our args are negative */
    if(num_slaves < 0 || timeout < 0) {
        RETURN_FALSE;
    }

    /* Grab our socket */
    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    // Construct the command
    cmd_len = REDIS_SPPRINTF(&cmd, "WAIT", "ll", num_slaves, timeout);

    /* Kick it off */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if (IS_ATOMIC(redis_sock)) {
        redis_long_response(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock, NULL,
            NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_long_response);
}

/* Construct a PUBSUB command */
PHP_REDIS_API int
redis_build_pubsub_cmd(RedisSock *redis_sock, char **ret, PUBSUB_TYPE type,
                       zval *arg)
{
    HashTable *ht_chan;
    zval *z_ele;
    smart_string cmd = {0};

    if (type == PUBSUB_CHANNELS) {
        if (arg) {
            /* With a pattern */
            return REDIS_SPPRINTF(ret, "PUBSUB", "sk", "CHANNELS", sizeof("CHANNELS") - 1,
                                  Z_STRVAL_P(arg), Z_STRLEN_P(arg));
        } else {
            /* No pattern */
            return REDIS_SPPRINTF(ret, "PUBSUB", "s", "CHANNELS", sizeof("CHANNELS") - 1);
        }
    } else if (type == PUBSUB_NUMSUB) {
        ht_chan = Z_ARRVAL_P(arg);

        // Add PUBSUB and NUMSUB bits
        redis_cmd_init_sstr(&cmd, zend_hash_num_elements(ht_chan)+1, "PUBSUB", sizeof("PUBSUB")-1);
        redis_cmd_append_sstr(&cmd, "NUMSUB", sizeof("NUMSUB")-1);

        /* Iterate our elements */
        ZEND_HASH_FOREACH_VAL(ht_chan, z_ele) {
            zend_string *zstr = zval_get_string(z_ele);
            redis_cmd_append_sstr_key(&cmd, ZSTR_VAL(zstr), ZSTR_LEN(zstr), redis_sock, NULL);
            zend_string_release(zstr);
        } ZEND_HASH_FOREACH_END();

        /* Set return */
        *ret = cmd.c;
        return cmd.len;
    } else if (type == PUBSUB_NUMPAT) {
        return REDIS_SPPRINTF(ret, "PUBSUB", "s", "NUMPAT", sizeof("NUMPAT") - 1);
    }

    /* Shouldn't ever happen */
    return -1;
}

/*
 * {{{ proto AwesomeRedis::pubsub("channels", pattern);
 *     proto AwesomeRedis::pubsub("numsub", Array channels);
 *     proto AwesomeRedis::pubsub("numpat"); }}}
 */
PHP_METHOD(AwesomeRedis, pubsub) {
    zval *object;
    RedisSock *redis_sock;
    char *keyword, *cmd;
    int cmd_len;
    size_t kw_len;
    PUBSUB_TYPE type;
    zval *arg = NULL;

    // Parse arguments
    if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                    "Os|z", &object, redis_ce, &keyword,
                                    &kw_len, &arg)==FAILURE)
    {
        RETURN_FALSE;
    }

    /* Validate our sub command keyword, and that we've got proper arguments */
    if(!strncasecmp(keyword, "channels", sizeof("channels"))) {
        /* One (optional) string argument */
        if(arg && Z_TYPE_P(arg) != IS_STRING) {
            RETURN_FALSE;
        }
        type = PUBSUB_CHANNELS;
    } else if(!strncasecmp(keyword, "numsub", sizeof("numsub"))) {
        /* One array argument */
        if(ZEND_NUM_ARGS() < 2 || Z_TYPE_P(arg) != IS_ARRAY ||
           zend_hash_num_elements(Z_ARRVAL_P(arg)) == 0)
        {
            RETURN_FALSE;
        }
        type = PUBSUB_NUMSUB;
    } else if(!strncasecmp(keyword, "numpat", sizeof("numpat"))) {
        type = PUBSUB_NUMPAT;
    } else {
        /* Invalid keyword */
        RETURN_FALSE;
    }

    /* Grab our socket context object */
    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    /* Construct our "PUBSUB" command */
    cmd_len = redis_build_pubsub_cmd(redis_sock, &cmd, type, arg);

    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);

    if(type == PUBSUB_NUMSUB) {
        if (IS_ATOMIC(redis_sock)) {
            if(redis_mbulk_reply_zipped_keys_int(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                                 redis_sock, NULL, NULL) < 0)
            {
                RETURN_FALSE;
            }
        }
        REDIS_PROCESS_RESPONSE(redis_mbulk_reply_zipped_keys_int);
    } else {
        if (IS_ATOMIC(redis_sock)) {
            if(redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                        redis_sock, NULL, NULL) < 0)
            {
                RETURN_FALSE;
            }
        }
        REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
    }
}

/* {{{ proto variant AwesomeRedis::eval(string script, [array keys, long num_keys]) */
PHP_METHOD(AwesomeRedis, eval)
{
    REDIS_PROCESS_KW_CMD("EVAL", redis_eval_cmd, redis_read_raw_variant_reply);
}

/* {{{ proto variant AwesomeRedis::evalsha(string sha1, [array keys, long num_keys]) */
PHP_METHOD(AwesomeRedis, evalsha) {
    REDIS_PROCESS_KW_CMD("EVALSHA", redis_eval_cmd, redis_read_raw_variant_reply);
}

/* {{{ proto status AwesomeRedis::script('flush')
 * {{{ proto status AwesomeRedis::script('kill')
 * {{{ proto string AwesomeRedis::script('load', lua_script)
 * {{{ proto int Reids::script('exists', script_sha1 [, script_sha2, ...])
 */
PHP_METHOD(AwesomeRedis, script) {
    zval *z_args;
    RedisSock *redis_sock;
    smart_string cmd = {0};
    int argc = ZEND_NUM_ARGS();

    /* Attempt to grab our socket */
    if (argc < 1 || (redis_sock = redis_sock_get(getThis(), 0)) == NULL) {
        RETURN_FALSE;
    }

    /* Allocate an array big enough to store our arguments */
    z_args = ecalloc(argc, sizeof(zval));

    /* Make sure we can grab our arguments, we have a string directive */
    if (zend_get_parameters_array(ht, argc, z_args) == FAILURE ||
        redis_build_script_cmd(&cmd, argc, z_args) == NULL
    ) {
        efree(z_args);
        RETURN_FALSE;
    }

    /* Free our alocated arguments */
    efree(z_args);

    // Kick off our request
    REDIS_PROCESS_REQUEST(redis_sock, cmd.c, cmd.len);
    if (IS_ATOMIC(redis_sock)) {
        if(redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                    redis_sock, NULL, NULL) < 0)
        {
            RETURN_FALSE;
        }
    }
    REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}

/* {{{ proto DUMP key */
PHP_METHOD(AwesomeRedis, dump) {
    REDIS_PROCESS_KW_CMD("DUMP", redis_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto AwesomeRedis::restore(ttl, key, value) */
PHP_METHOD(AwesomeRedis, restore) {
    REDIS_PROCESS_KW_CMD("RESTORE", redis_key_long_val_cmd,
        redis_boolean_response);
}
/* }}} */

/* {{{ proto AwesomeRedis::debug(string key) */
PHP_METHOD(AwesomeRedis, debug) {
    REDIS_PROCESS_KW_CMD("DEBUG", redis_key_cmd, redis_string_response);
}
/* }}} */

/* {{{ proto AwesomeRedis::migrate(host port key dest-db timeout [bool copy,
 *                          bool replace]) */
PHP_METHOD(AwesomeRedis, migrate) {
    REDIS_PROCESS_CMD(migrate, redis_boolean_response);
}

/* {{{ proto AwesomeRedis::_prefix(key) */
PHP_METHOD(AwesomeRedis, _prefix) {
    RedisSock *redis_sock;

    if ((redis_sock = redis_sock_get_instance(getThis(), 0)) == NULL) {
        RETURN_FALSE;
    }

    redis_prefix_handler(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock);
}

/* {{{ proto AwesomeRedis::_serialize(value) */
PHP_METHOD(AwesomeRedis, _serialize) {
    RedisSock *redis_sock;

    // Grab socket
    if ((redis_sock = redis_sock_get_instance(getThis(), 0)) == NULL) {
        RETURN_FALSE;
    }

    redis_serialize_handler(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock);
}

/* {{{ proto AwesomeRedis::_unserialize(value) */
PHP_METHOD(AwesomeRedis, _unserialize) {
    RedisSock *redis_sock;

    // Grab socket
    if ((redis_sock = redis_sock_get_instance(getThis(), 0)) == NULL) {
        RETURN_FALSE;
    }

    redis_unserialize_handler(INTERNAL_FUNCTION_PARAM_PASSTHRU, redis_sock,
        redis_exception_ce);
}

/* {{{ proto AwesomeRedis::getLastError() */
PHP_METHOD(AwesomeRedis, getLastError) {
    zval *object;
    RedisSock *redis_sock;

    // Grab our object
    if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "O",
                                    &object, redis_ce) == FAILURE)
    {
        RETURN_FALSE;
    }

    // Grab socket
    if ((redis_sock = redis_sock_get_instance(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    /* Return our last error or NULL if we don't have one */
    if (redis_sock->err) {
        RETURN_STRINGL(ZSTR_VAL(redis_sock->err), ZSTR_LEN(redis_sock->err));
    }
    RETURN_NULL();
}

/* {{{ proto AwesomeRedis::clearLastError() */
PHP_METHOD(AwesomeRedis, clearLastError) {
    zval *object;
    RedisSock *redis_sock;

    // Grab our object
    if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "O",
                                    &object, redis_ce) == FAILURE)
    {
        RETURN_FALSE;
    }
    // Grab socket
    if ((redis_sock = redis_sock_get_instance(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    // Clear error message
    if (redis_sock->err) {
        zend_string_release(redis_sock->err);
        redis_sock->err = NULL;
    }

    RETURN_TRUE;
}

/*
 * {{{ proto long AwesomeRedis::getMode()
 */
PHP_METHOD(AwesomeRedis, getMode) {
    zval *object;
    RedisSock *redis_sock;

    /* Grab our object */
    if (zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "O", &object, redis_ce) == FAILURE) {
        RETURN_FALSE;
    }

    /* Grab socket */
    if ((redis_sock = redis_sock_get_instance(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    if (IS_PIPELINE(redis_sock)) {
        RETVAL_LONG(PIPELINE);
    } else if (IS_MULTI(redis_sock)) {
        RETVAL_LONG(MULTI);
    } else {
        RETVAL_LONG(ATOMIC);
    }
}

/* {{{ proto AwesomeRedis::time() */
PHP_METHOD(AwesomeRedis, time) {
    REDIS_PROCESS_KW_CMD("TIME", redis_empty_cmd, redis_mbulk_reply_raw);
}

/* {{{ proto array AwesomeRedis::role() */
PHP_METHOD(AwesomeRedis, role) {
    REDIS_PROCESS_KW_CMD("ROLE", redis_empty_cmd, redis_read_variant_reply);
}

/*
 * Introspection stuff
 */

/* {{{ proto AwesomeRedis::IsConnected */
PHP_METHOD(AwesomeRedis, isConnected) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        RETURN_TRUE;
    } else {
        RETURN_FALSE;
    }
}

/* {{{ proto AwesomeRedis::getHost() */
PHP_METHOD(AwesomeRedis, getHost) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        RETURN_STRINGL(ZSTR_VAL(redis_sock->host), ZSTR_LEN(redis_sock->host));
    } else {
        RETURN_FALSE;
    }
}

/* {{{ proto AwesomeRedis::getPort() */
PHP_METHOD(AwesomeRedis, getPort) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        /* Return our port */
        RETURN_LONG(redis_sock->port);
    } else {
        RETURN_FALSE;
    }
}

/* {{{ proto AwesomeRedis::getDBNum */
PHP_METHOD(AwesomeRedis, getDBNum) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        /* Return our db number */
        RETURN_LONG(redis_sock->dbNumber);
    } else {
        RETURN_FALSE;
    }
}

/* {{{ proto AwesomeRedis::getTimeout */
PHP_METHOD(AwesomeRedis, getTimeout) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        RETURN_DOUBLE(redis_sock->timeout);
    } else {
        RETURN_FALSE;
    }
}

/* {{{ proto AwesomeRedis::getReadTimeout */
PHP_METHOD(AwesomeRedis, getReadTimeout) {
    RedisSock *redis_sock;

    if((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU))) {
        RETURN_DOUBLE(redis_sock->read_timeout);
    } else {
        RETURN_FALSE;
    }
}

/* {{{ proto AwesomeRedis::getPersistentID */
PHP_METHOD(AwesomeRedis, getPersistentID) {
    RedisSock *redis_sock;

    if ((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU)) == NULL) {
        RETURN_FALSE;
    } else if (redis_sock->persistent_id == NULL) {
        RETURN_NULL();
    }
    RETURN_STRINGL(ZSTR_VAL(redis_sock->persistent_id), ZSTR_LEN(redis_sock->persistent_id));
}

/* {{{ proto AwesomeRedis::getAuth */
PHP_METHOD(AwesomeRedis, getAuth) {
    RedisSock *redis_sock;

    if ((redis_sock = redis_sock_get_connected(INTERNAL_FUNCTION_PARAM_PASSTHRU)) == NULL) {
        RETURN_FALSE;
    } else if (redis_sock->auth == NULL) {
        RETURN_NULL();
    }
    RETURN_STRINGL(ZSTR_VAL(redis_sock->auth), ZSTR_LEN(redis_sock->auth));
}

/*
 * $redis->client('list');
 * $redis->client('kill', <ip:port>);
 * $redis->client('setname', <name>);
 * $redis->client('getname');
 */
PHP_METHOD(AwesomeRedis, client) {
    zval *object;
    RedisSock *redis_sock;
    char *cmd, *opt = NULL, *arg = NULL;
    size_t opt_len, arg_len;
    int cmd_len;

    // Parse our method parameters
    if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                    "Os|s", &object, redis_ce, &opt, &opt_len,
                                    &arg, &arg_len) == FAILURE)
    {
        RETURN_FALSE;
    }

    /* Grab our socket */
    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    /* Build our CLIENT command */
    if (ZEND_NUM_ARGS() == 2) {
        cmd_len = REDIS_SPPRINTF(&cmd, "CLIENT", "ss", opt, opt_len, arg, arg_len);
    } else {
        cmd_len = REDIS_SPPRINTF(&cmd, "CLIENT", "s", opt, opt_len);
    }

    /* Execute our queue command */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);

    /* We handle CLIENT LIST with a custom response function */
    if(!strncasecmp(opt, "list", 4)) {
        if (IS_ATOMIC(redis_sock)) {
            redis_client_list_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,redis_sock,
                NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_client_list_reply);
    } else {
        if (IS_ATOMIC(redis_sock)) {
            redis_read_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                redis_sock,NULL,NULL);
        }
        REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
    }
}

/* {{{ proto mixed AwesomeRedis::rawcommand(string $command, [ $arg1 ... $argN]) */
PHP_METHOD(AwesomeRedis, rawcommand) {
    int argc = ZEND_NUM_ARGS(), cmd_len;
    char *cmd = NULL;
    RedisSock *redis_sock;
    zval *z_args;

    /* Sanity check on arguments */
    if (argc < 1) {
        php_error_docref(NULL, E_WARNING,
            "Must pass at least one command keyword");
        RETURN_FALSE;
    }
    z_args = emalloc(argc * sizeof(zval));
    if (zend_get_parameters_array(ht, argc, z_args) == FAILURE) {
        php_error_docref(NULL, E_WARNING,
            "Internal PHP error parsing arguments");
        efree(z_args);
        RETURN_FALSE;
    } else if (redis_build_raw_cmd(z_args, argc, &cmd, &cmd_len) < 0 ||
               (redis_sock = redis_sock_get(getThis(), 0)) == NULL
    ) {
        if (cmd) efree(cmd);
        efree(z_args);
        RETURN_FALSE;
    }

    /* Clean up command array */
    efree(z_args);

    /* Execute our command */
    REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
    if (IS_ATOMIC(redis_sock)) {
        redis_read_raw_variant_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,redis_sock,NULL,NULL);
    }
    REDIS_PROCESS_RESPONSE(redis_read_variant_reply);
}
/* }}} */

/* {{{ proto array AwesomeRedis::command()
 *     proto array AwesomeRedis::command('info', string cmd)
 *     proto array AwesomeRedis::command('getkeys', array cmd_args) */
PHP_METHOD(AwesomeRedis, command) {
    REDIS_PROCESS_CMD(command, redis_read_variant_reply);
}
/* }}} */

/* Helper to format any combination of SCAN arguments */
PHP_REDIS_API int
redis_build_scan_cmd(char **cmd, REDIS_SCAN_TYPE type, char *key, int key_len,
                     int iter, char *pattern, int pattern_len, int count)
{
    smart_string cmdstr = {0};
    char *keyword;
    int argc;

    /* Count our arguments +1 for key if it's got one, and + 2 for pattern */
    /* or count given that they each carry keywords with them. */
    argc = 1 + (key_len > 0) + (pattern_len > 0 ? 2 : 0) + (count > 0 ? 2 : 0);

    /* Turn our type into a keyword */
    switch(type) {
        case TYPE_SCAN:
            keyword = "SCAN";
            break;
        case TYPE_SSCAN:
            keyword = "SSCAN";
            break;
        case TYPE_HSCAN:
            keyword = "HSCAN";
            break;
        case TYPE_ZSCAN:
        default:
            keyword = "ZSCAN";
            break;
    }

    /* Start the command */
    redis_cmd_init_sstr(&cmdstr, argc, keyword, strlen(keyword));
    if (key_len) redis_cmd_append_sstr(&cmdstr, key, key_len);
    redis_cmd_append_sstr_int(&cmdstr, iter);

    /* Append COUNT if we've got it */
    if(count) {
        REDIS_CMD_APPEND_SSTR_STATIC(&cmdstr, "COUNT");
        redis_cmd_append_sstr_int(&cmdstr, count);
    }

    /* Append MATCH if we've got it */
    if(pattern_len) {
        REDIS_CMD_APPEND_SSTR_STATIC(&cmdstr, "MATCH");
        redis_cmd_append_sstr(&cmdstr, pattern, pattern_len);
    }

    /* Return our command length */
    *cmd = cmdstr.c;
    return cmdstr.len;
}

/* {{{ proto AwesomeRedis::scan(&$iterator, [pattern, [count]]) */
PHP_REDIS_API void
generic_scan_cmd(INTERNAL_FUNCTION_PARAMETERS, REDIS_SCAN_TYPE type) {
    zval *object, *z_iter;
    RedisSock *redis_sock;
    HashTable *hash;
    char *pattern = NULL, *cmd, *key = NULL;
    int cmd_len, num_elements, key_free = 0;
    size_t key_len = 0, pattern_len = 0;
    zend_long count = 0, iter;

    /* Different prototype depending on if this is a key based scan */
    if(type != TYPE_SCAN) {
        // Requires a key
        if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                        "Osz/|s!l", &object, redis_ce, &key,
                                        &key_len, &z_iter, &pattern,
                                        &pattern_len, &count)==FAILURE)
        {
            RETURN_FALSE;
        }
    } else {
        // Doesn't require a key
        if(zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(),
                                        "Oz/|s!l", &object, redis_ce, &z_iter,
                                        &pattern, &pattern_len, &count)
                                        == FAILURE)
        {
            RETURN_FALSE;
        }
    }

    /* Grab our socket */
    if ((redis_sock = redis_sock_get(object, 0)) == NULL) {
        RETURN_FALSE;
    }

    /* Calling this in a pipeline makes no sense */
    if (!IS_ATOMIC(redis_sock)) {
        php_error_docref(NULL, E_ERROR,
            "Can't call SCAN commands in multi or pipeline mode!");
        RETURN_FALSE;
    }

    // The iterator should be passed in as NULL for the first iteration, but we
    // can treat any NON LONG value as NULL for these purposes as we've
    // seperated the variable anyway.
    if(Z_TYPE_P(z_iter) != IS_LONG || Z_LVAL_P(z_iter) < 0) {
        /* Convert to long */
        convert_to_long(z_iter);
        iter = 0;
    } else if(Z_LVAL_P(z_iter) != 0) {
        /* Update our iterator value for the next passthru */
        iter = Z_LVAL_P(z_iter);
    } else {
        /* We're done, back to iterator zero */
        RETURN_FALSE;
    }

    /* Prefix our key if we've got one and we have a prefix set */
    if(key_len) {
        key_free = redis_key_prefix(redis_sock, &key, &key_len);
    }

    /**
     * Redis can return to us empty keys, especially in the case where there
     * are a large number of keys to scan, and we're matching against a
     * pattern.  phpredis can be set up to abstract this from the user, by
     * setting OPT_SCAN to REDIS_SCAN_RETRY.  Otherwise we will return empty
     * keys and the user will need to make subsequent calls with an updated
     * iterator.
     */
    do {
        /* Free our previous reply if we're back in the loop.  We know we are
         * if our return_value is an array */
        if (Z_TYPE_P(return_value) == IS_ARRAY) {
            zval_dtor(return_value);
            ZVAL_NULL(return_value);
        }

        // Format our SCAN command
        cmd_len = redis_build_scan_cmd(&cmd, type, key, key_len, (int)iter,
                                   pattern, pattern_len, count);

        /* Execute our command getting our new iterator value */
        REDIS_PROCESS_REQUEST(redis_sock, cmd, cmd_len);
        if(redis_sock_read_scan_reply(INTERNAL_FUNCTION_PARAM_PASSTHRU,
                                      redis_sock,type,&iter) < 0)
        {
            if(key_free) efree(key);
            RETURN_FALSE;
        }

        /* Get the number of elements */
        hash = Z_ARRVAL_P(return_value);
        num_elements = zend_hash_num_elements(hash);
    } while(redis_sock->scan == REDIS_SCAN_RETRY && iter != 0 &&
            num_elements == 0);

    /* Free our key if it was prefixed */
    if(key_free) efree(key);

    /* Update our iterator reference */
    Z_LVAL_P(z_iter) = iter;
}

PHP_METHOD(AwesomeRedis, scan) {
    generic_scan_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, TYPE_SCAN);
}
PHP_METHOD(AwesomeRedis, hscan) {
    generic_scan_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, TYPE_HSCAN);
}
PHP_METHOD(AwesomeRedis, sscan) {
    generic_scan_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, TYPE_SSCAN);
}
PHP_METHOD(AwesomeRedis, zscan) {
    generic_scan_cmd(INTERNAL_FUNCTION_PARAM_PASSTHRU, TYPE_ZSCAN);
}

/*
 * HyperLogLog based commands
 */

/* {{{ proto AwesomeRedis::pfAdd(string key, array elements) }}} */
PHP_METHOD(AwesomeRedis, pfadd) {
    REDIS_PROCESS_CMD(pfadd, redis_long_response);
}

/* {{{ proto AwesomeRedis::pfCount(string key) }}}*/
PHP_METHOD(AwesomeRedis, pfcount) {
    REDIS_PROCESS_CMD(pfcount, redis_long_response);
}

/* {{{ proto AwesomeRedis::pfMerge(string dstkey, array keys) }}}*/
PHP_METHOD(AwesomeRedis, pfmerge) {
    REDIS_PROCESS_CMD(pfmerge, redis_boolean_response);
}

/*
 * Geo commands
 */

PHP_METHOD(AwesomeRedis, geoadd) {
    REDIS_PROCESS_KW_CMD("GEOADD", redis_key_varval_cmd, redis_long_response);
}

PHP_METHOD(AwesomeRedis, geohash) {
    REDIS_PROCESS_KW_CMD("GEOHASH", redis_key_varval_cmd, redis_mbulk_reply_raw);
}

PHP_METHOD(AwesomeRedis, geopos) {
    REDIS_PROCESS_KW_CMD("GEOPOS", redis_key_varval_cmd, redis_read_variant_reply);
}

PHP_METHOD(AwesomeRedis, geodist) {
    REDIS_PROCESS_CMD(geodist, redis_bulk_double_response);
}

PHP_METHOD(AwesomeRedis, georadius) {
    REDIS_PROCESS_KW_CMD("GEORADIUS", redis_georadius_cmd, redis_read_variant_reply);
}

PHP_METHOD(AwesomeRedis, georadius_ro) {
    REDIS_PROCESS_KW_CMD("GEORADIUS_RO", redis_georadius_cmd, redis_read_variant_reply);
}

PHP_METHOD(AwesomeRedis, georadiusbymember) {
    REDIS_PROCESS_KW_CMD("GEORADIUSBYMEMBER", redis_georadiusbymember_cmd, redis_read_variant_reply);
}

PHP_METHOD(AwesomeRedis, georadiusbymember_ro) {
    REDIS_PROCESS_KW_CMD("GEORADIUSBYMEMBER_RO", redis_georadiusbymember_cmd, redis_read_variant_reply);
}

/*
 * Streams
 */

PHP_METHOD(AwesomeRedis, xack) {
    REDIS_PROCESS_CMD(xack, redis_long_response);
}

PHP_METHOD(AwesomeRedis, xadd) {
    REDIS_PROCESS_CMD(xadd, redis_read_variant_reply);
}

PHP_METHOD(AwesomeRedis, xclaim) {
    REDIS_PROCESS_CMD(xclaim, redis_xclaim_reply);
}

PHP_METHOD(AwesomeRedis, xdel) {
    REDIS_PROCESS_KW_CMD("XDEL", redis_key_str_arr_cmd, redis_long_response);
}

PHP_METHOD(AwesomeRedis, xgroup) {
    REDIS_PROCESS_CMD(xgroup, redis_read_variant_reply);
}

PHP_METHOD(AwesomeRedis, xinfo) {
    REDIS_PROCESS_CMD(xinfo, redis_xinfo_reply);
}

PHP_METHOD(AwesomeRedis, xlen) {
    REDIS_PROCESS_KW_CMD("XLEN", redis_key_cmd, redis_long_response);
}

PHP_METHOD(AwesomeRedis, xpending) {
    REDIS_PROCESS_CMD(xpending, redis_read_variant_reply_strings);
}

PHP_METHOD(AwesomeRedis, xrange) {
    REDIS_PROCESS_KW_CMD("XRANGE", redis_xrange_cmd, redis_xrange_reply);
}

PHP_METHOD(AwesomeRedis, xread) {
    REDIS_PROCESS_CMD(xread, redis_xread_reply);
}

PHP_METHOD(AwesomeRedis, xreadgroup) {
    REDIS_PROCESS_CMD(xreadgroup, redis_xread_reply);
}

PHP_METHOD(AwesomeRedis, xrevrange) {
    REDIS_PROCESS_KW_CMD("XREVRANGE", redis_xrange_cmd, redis_xrange_reply);
}

PHP_METHOD(AwesomeRedis, xtrim) {
    REDIS_PROCESS_CMD(xtrim, redis_long_response);
}

/* vim: set tabstop=4 softtabstop=4 expandtab shiftwidth=4: */
