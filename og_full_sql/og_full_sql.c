// clang-format off
#include <postgres.h>
#include <fmgr.h>
// clang-format on
#include "executor/spi.h"
#include "executor/tstoreReceiver.h"
#include "funcapi.h"
#include "nodes/nodes.h"
#include "utils/builtins.h"
#include <miscadmin.h>
#include "utils/timestamp.h"
#include "postgres.h"
#include "utils/elog.h"
#include "tcop/utility.h"

static THR_LOCAL ExecutorRun_hook_type ExecutorRun_old_hook = NULL;
static THR_LOCAL ProcessUtility_hook_type ProcessUtility_old_hook = NULL;
static char *last_query = NULL;

static void ogProcessUtility_hook(Node* parsetree, const char* queryString, ParamListInfo params,
    bool isTopLevel, DestReceiver* dest,
#ifdef PGXC
    bool sentToRemote,
#endif /* PGXC */
    char* completionTag,
    bool isCTAS) {
    Query *query = (Query *)parsetree;
    // char *queryString = nodeToString(query);
    ereport(LOG, (errmsg("[full sql]ProcessUtility_hook: %s, Transaction ID: %u", queryString,GetCurrentTransactionId())));
    standard_ProcessUtility(parsetree, queryString, params, isTopLevel, dest, 
#ifdef PGXC
sentToRemote, 
#endif /* PGXC */
completionTag, isCTAS);
}

static void ogExecutorRun_hook(QueryDesc* queryDesc, ScanDirection direction, long count) {
    TransactionId xid = GetCurrentTransactionId();
    ereport(LOG, (errmsg("[full sql]ExecutorRun_hook: %s, Transaction ID: %u, paramlist: %x", queryDesc->sourceText, xid, queryDesc->params)));
    standard_ExecutorRun(queryDesc, direction, count);
}


void _PG_init(void);

void _PG_init(void) {
    ProcessUtility_old_hook = ProcessUtility_hook;
    ProcessUtility_hook = ogProcessUtility_hook;

    ExecutorRun_old_hook = ExecutorRun_hook;
    ExecutorRun_hook = ogExecutorRun_hook;
}

PG_MODULE_MAGIC;