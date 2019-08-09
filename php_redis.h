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
  | Maintainer: Michael Grunder <michael.grunder@gmail.com>              |
  | Maintainer: Nasreddine Bouafif <n.bouafif@owlient.eu>                |
  +----------------------------------------------------------------------+
*/

#include "common.h"

#ifndef PHP_REDIS_H
#define PHP_REDIS_H

/* phpredis version */
#define PHP_REDIS_VERSION "5.1.0-dev"

PHP_METHOD(AwesomeRedis, __construct);
PHP_METHOD(AwesomeRedis, __destruct);
PHP_METHOD(AwesomeRedis, append);
PHP_METHOD(AwesomeRedis, auth);
PHP_METHOD(AwesomeRedis, bgSave);
PHP_METHOD(AwesomeRedis, bgrewriteaof);
PHP_METHOD(AwesomeRedis, bitcount);
PHP_METHOD(AwesomeRedis, bitop);
PHP_METHOD(AwesomeRedis, bitpos);
PHP_METHOD(AwesomeRedis, blPop);
PHP_METHOD(AwesomeRedis, brPop);
PHP_METHOD(AwesomeRedis, bzPopMax);
PHP_METHOD(AwesomeRedis, bzPopMin);
PHP_METHOD(AwesomeRedis, close);
PHP_METHOD(AwesomeRedis, connect);
PHP_METHOD(AwesomeRedis, dbSize);
PHP_METHOD(AwesomeRedis, decr);
PHP_METHOD(AwesomeRedis, decrBy);
PHP_METHOD(AwesomeRedis, del);
PHP_METHOD(AwesomeRedis, echo);
PHP_METHOD(AwesomeRedis, exists);
PHP_METHOD(AwesomeRedis, expire);
PHP_METHOD(AwesomeRedis, expireAt);
PHP_METHOD(AwesomeRedis, flushAll);
PHP_METHOD(AwesomeRedis, flushDB);
PHP_METHOD(AwesomeRedis, get);
PHP_METHOD(AwesomeRedis, getBit);
PHP_METHOD(AwesomeRedis, getRange);
PHP_METHOD(AwesomeRedis, getSet);
PHP_METHOD(AwesomeRedis, incr);
PHP_METHOD(AwesomeRedis, incrBy);
PHP_METHOD(AwesomeRedis, incrByFloat);
PHP_METHOD(AwesomeRedis, info);
PHP_METHOD(AwesomeRedis, keys);
PHP_METHOD(AwesomeRedis, lInsert);
PHP_METHOD(AwesomeRedis, lLen);
PHP_METHOD(AwesomeRedis, lPop);
PHP_METHOD(AwesomeRedis, lPush);
PHP_METHOD(AwesomeRedis, lPushx);
PHP_METHOD(AwesomeRedis, lSet);
PHP_METHOD(AwesomeRedis, lastSave);
PHP_METHOD(AwesomeRedis, lindex);
PHP_METHOD(AwesomeRedis, lrange);
PHP_METHOD(AwesomeRedis, lrem);
PHP_METHOD(AwesomeRedis, ltrim);
PHP_METHOD(AwesomeRedis, mget);
PHP_METHOD(AwesomeRedis, move);
PHP_METHOD(AwesomeRedis, object);
PHP_METHOD(AwesomeRedis, pconnect);
PHP_METHOD(AwesomeRedis, persist);
PHP_METHOD(AwesomeRedis, pexpire);
PHP_METHOD(AwesomeRedis, pexpireAt);
PHP_METHOD(AwesomeRedis, ping);
PHP_METHOD(AwesomeRedis, psetex);
PHP_METHOD(AwesomeRedis, pttl);
PHP_METHOD(AwesomeRedis, rPop);
PHP_METHOD(AwesomeRedis, rPush);
PHP_METHOD(AwesomeRedis, rPushx);
PHP_METHOD(AwesomeRedis, randomKey);
PHP_METHOD(AwesomeRedis, rename);
PHP_METHOD(AwesomeRedis, renameNx);
PHP_METHOD(AwesomeRedis, sAdd);
PHP_METHOD(AwesomeRedis, sAddArray);
PHP_METHOD(AwesomeRedis, sDiff);
PHP_METHOD(AwesomeRedis, sDiffStore);
PHP_METHOD(AwesomeRedis, sInter);
PHP_METHOD(AwesomeRedis, sInterStore);
PHP_METHOD(AwesomeRedis, sMembers);
PHP_METHOD(AwesomeRedis, sMove);
PHP_METHOD(AwesomeRedis, sPop);
PHP_METHOD(AwesomeRedis, sRandMember);
PHP_METHOD(AwesomeRedis, sUnion);
PHP_METHOD(AwesomeRedis, sUnionStore);
PHP_METHOD(AwesomeRedis, save);
PHP_METHOD(AwesomeRedis, scard);
PHP_METHOD(AwesomeRedis, select);
PHP_METHOD(AwesomeRedis, set);
PHP_METHOD(AwesomeRedis, setBit);
PHP_METHOD(AwesomeRedis, setRange);
PHP_METHOD(AwesomeRedis, setex);
PHP_METHOD(AwesomeRedis, setnx);
PHP_METHOD(AwesomeRedis, sismember);
PHP_METHOD(AwesomeRedis, slaveof);
PHP_METHOD(AwesomeRedis, sort);
PHP_METHOD(AwesomeRedis, sortAsc);
PHP_METHOD(AwesomeRedis, sortAscAlpha);
PHP_METHOD(AwesomeRedis, sortDesc);
PHP_METHOD(AwesomeRedis, sortDescAlpha);
PHP_METHOD(AwesomeRedis, srem);
PHP_METHOD(AwesomeRedis, strlen);
PHP_METHOD(AwesomeRedis, swapdb);
PHP_METHOD(AwesomeRedis, ttl);
PHP_METHOD(AwesomeRedis, type);
PHP_METHOD(AwesomeRedis, unlink);
PHP_METHOD(AwesomeRedis, zAdd);
PHP_METHOD(AwesomeRedis, zCard);
PHP_METHOD(AwesomeRedis, zCount);
PHP_METHOD(AwesomeRedis, zIncrBy);
PHP_METHOD(AwesomeRedis, zLexCount);
PHP_METHOD(AwesomeRedis, zPopMax);
PHP_METHOD(AwesomeRedis, zPopMin);
PHP_METHOD(AwesomeRedis, zRange);
PHP_METHOD(AwesomeRedis, zRangeByLex);
PHP_METHOD(AwesomeRedis, zRangeByScore);
PHP_METHOD(AwesomeRedis, zRank);
PHP_METHOD(AwesomeRedis, zRem);
PHP_METHOD(AwesomeRedis, zRemRangeByLex);
PHP_METHOD(AwesomeRedis, zRemRangeByRank);
PHP_METHOD(AwesomeRedis, zRemRangeByScore);
PHP_METHOD(AwesomeRedis, zRevRange);
PHP_METHOD(AwesomeRedis, zRevRangeByLex);
PHP_METHOD(AwesomeRedis, zRevRangeByScore);
PHP_METHOD(AwesomeRedis, zRevRank);
PHP_METHOD(AwesomeRedis, zScore);
PHP_METHOD(AwesomeRedis, zinterstore);
PHP_METHOD(AwesomeRedis, zunionstore);

PHP_METHOD(AwesomeRedis, eval);
PHP_METHOD(AwesomeRedis, evalsha);
PHP_METHOD(AwesomeRedis, script);
PHP_METHOD(AwesomeRedis, debug);
PHP_METHOD(AwesomeRedis, dump);
PHP_METHOD(AwesomeRedis, restore);
PHP_METHOD(AwesomeRedis, migrate);

PHP_METHOD(AwesomeRedis, time);
PHP_METHOD(AwesomeRedis, role);

PHP_METHOD(AwesomeRedis, getLastError);
PHP_METHOD(AwesomeRedis, clearLastError);
PHP_METHOD(AwesomeRedis, _prefix);
PHP_METHOD(AwesomeRedis, _serialize);
PHP_METHOD(AwesomeRedis, _unserialize);

PHP_METHOD(AwesomeRedis, mset);
PHP_METHOD(AwesomeRedis, msetnx);
PHP_METHOD(AwesomeRedis, rpoplpush);
PHP_METHOD(AwesomeRedis, brpoplpush);

PHP_METHOD(AwesomeRedis, hGet);
PHP_METHOD(AwesomeRedis, hSet);
PHP_METHOD(AwesomeRedis, hSetNx);
PHP_METHOD(AwesomeRedis, hDel);
PHP_METHOD(AwesomeRedis, hLen);
PHP_METHOD(AwesomeRedis, hKeys);
PHP_METHOD(AwesomeRedis, hVals);
PHP_METHOD(AwesomeRedis, hGetAll);
PHP_METHOD(AwesomeRedis, hExists);
PHP_METHOD(AwesomeRedis, hIncrBy);
PHP_METHOD(AwesomeRedis, hIncrByFloat);
PHP_METHOD(AwesomeRedis, hMset);
PHP_METHOD(AwesomeRedis, hMget);
PHP_METHOD(AwesomeRedis, hStrLen);

PHP_METHOD(AwesomeRedis, multi);
PHP_METHOD(AwesomeRedis, discard);
PHP_METHOD(AwesomeRedis, exec);
PHP_METHOD(AwesomeRedis, watch);
PHP_METHOD(AwesomeRedis, unwatch);

PHP_METHOD(AwesomeRedis, pipeline);

PHP_METHOD(AwesomeRedis, publish);
PHP_METHOD(AwesomeRedis, subscribe);
PHP_METHOD(AwesomeRedis, psubscribe);
PHP_METHOD(AwesomeRedis, unsubscribe);
PHP_METHOD(AwesomeRedis, punsubscribe);

PHP_METHOD(AwesomeRedis, getOption);
PHP_METHOD(AwesomeRedis, setOption);

PHP_METHOD(AwesomeRedis, config);
PHP_METHOD(AwesomeRedis, slowlog);
PHP_METHOD(AwesomeRedis, wait);
PHP_METHOD(AwesomeRedis, pubsub);

/* Geoadd and friends */
PHP_METHOD(AwesomeRedis, geoadd);
PHP_METHOD(AwesomeRedis, geohash);
PHP_METHOD(AwesomeRedis, geopos);
PHP_METHOD(AwesomeRedis, geodist);
PHP_METHOD(AwesomeRedis, georadius);
PHP_METHOD(AwesomeRedis, georadius_ro);
PHP_METHOD(AwesomeRedis, georadiusbymember);
PHP_METHOD(AwesomeRedis, georadiusbymember_ro);

PHP_METHOD(AwesomeRedis, client);
PHP_METHOD(AwesomeRedis, command);
PHP_METHOD(AwesomeRedis, rawcommand);

/* SCAN and friends  */
PHP_METHOD(AwesomeRedis, scan);
PHP_METHOD(AwesomeRedis, hscan);
PHP_METHOD(AwesomeRedis, sscan);
PHP_METHOD(AwesomeRedis, zscan);

/* HyperLogLog commands */
PHP_METHOD(AwesomeRedis, pfadd);
PHP_METHOD(AwesomeRedis, pfcount);
PHP_METHOD(AwesomeRedis, pfmerge);

/* STREAMS */
PHP_METHOD(AwesomeRedis, xack);
PHP_METHOD(AwesomeRedis, xadd);
PHP_METHOD(AwesomeRedis, xclaim);
PHP_METHOD(AwesomeRedis, xdel);
PHP_METHOD(AwesomeRedis, xgroup);
PHP_METHOD(AwesomeRedis, xinfo);
PHP_METHOD(AwesomeRedis, xlen);
PHP_METHOD(AwesomeRedis, xpending);
PHP_METHOD(AwesomeRedis, xrange);
PHP_METHOD(AwesomeRedis, xread);
PHP_METHOD(AwesomeRedis, xreadgroup);
PHP_METHOD(AwesomeRedis, xrevrange);
PHP_METHOD(AwesomeRedis, xtrim);

/* Reflection */
PHP_METHOD(AwesomeRedis, getHost);
PHP_METHOD(AwesomeRedis, getPort);
PHP_METHOD(AwesomeRedis, getDBNum);
PHP_METHOD(AwesomeRedis, getTimeout);
PHP_METHOD(AwesomeRedis, getReadTimeout);
PHP_METHOD(AwesomeRedis, isConnected);
PHP_METHOD(AwesomeRedis, getPersistentID);
PHP_METHOD(AwesomeRedis, getAuth);
PHP_METHOD(AwesomeRedis, getMode);

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(redis);
PHP_MSHUTDOWN_FUNCTION(redis);
PHP_MINFO_FUNCTION(redis);

/* Redis response handler function callback prototype */
typedef void (*ResultCallback)(INTERNAL_FUNCTION_PARAMETERS, 
    RedisSock *redis_sock, zval *z_tab, void *ctx);

PHP_REDIS_API int redis_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent);

PHP_REDIS_API void generic_subscribe_cmd(INTERNAL_FUNCTION_PARAMETERS, char *sub_cmd);

PHP_REDIS_API void generic_unsubscribe_cmd(INTERNAL_FUNCTION_PARAMETERS, 
    char *unsub_cmd);

PHP_REDIS_API int redis_response_enqueued(RedisSock *redis_sock);

PHP_REDIS_API int get_flag(zval *object);

PHP_REDIS_API void set_flag(zval *object, int new_flag);

PHP_REDIS_API int redis_sock_read_multibulk_multi_reply_loop(
    INTERNAL_FUNCTION_PARAMETERS, RedisSock *redis_sock, zval *z_tab, 
    int numElems);

extern zend_module_entry redis_module_entry;

#define redis_module_ptr &redis_module_entry
#define phpext_redis_ptr redis_module_ptr

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
