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
#include "lib/stringinfo.h"
#include "nodes/pg_list.h"

static THR_LOCAL ExecutorRun_hook_type ExecutorRun_old_hook = NULL;
static THR_LOCAL ProcessUtility_hook_type ProcessUtility_old_hook = NULL;

static char *tablename = "*";

static void ogProcessUtility_hook(Node* parsetree, const char* queryString, ParamListInfo params,
    bool isTopLevel, DestReceiver* dest,
#ifdef PGXC
    bool sentToRemote,
#endif /* PGXC */
    char* completionTag,
    bool isCTAS) {
    Query *query = (Query *)parsetree;
    // char *queryString = nodeToString(query);
    ereport(LOG, (errmsg("[full sql]ProcessUtility_hook: %s, Transaction ID: %u, tbn: %s", queryString,GetCurrentTransactionId(), tablename)));
    standard_ProcessUtility(parsetree, queryString, params, isTopLevel, dest, 
#ifdef PGXC
sentToRemote, 
#endif /* PGXC */
completionTag, isCTAS);
}

static void getTypeOutputInfo(Oid type, Oid* typOutput, bool* typIsVarlena)
{
    HeapTuple typeTuple;
    Form_pg_type pt;
    typeTuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(type));
    if (!HeapTupleIsValid(typeTuple)) {
        ereport(ERROR, (errcode(ERRCODE_CACHE_LOOKUP_FAILED), errmsg("cache lookup failed for type %u", type)));
    }
    pt = (Form_pg_type)GETSTRUCT(typeTuple);
    if (!pt->typisdefined) {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("type %s is only a shell", format_type_be(type))));
    }
    if (!OidIsValid(pt->typoutput)) {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION),
            errmsg("no output function available for type %s", format_type_be(type))));
    }
    *typOutput = pt->typoutput;
    *typIsVarlena = (!pt->typbyval) && (pt->typlen == -1);
    ReleaseSysCache(typeTuple);
}

static void ogExecutorRun_hook(QueryDesc* queryDesc, ScanDirection direction, long count) {
    bool isLog = false;
    if (strcmp(tablename, "*") == 0) {
        isLog = true;
    } else if (strcmp(tablename, "") == 0) {
        
    } else {
        List* list = queryDesc->plannedstmt->rtable;
        ListCell* lc = NULL;
        foreach (lc, list) {
            RangeTblEntry* rte = (RangeTblEntry*)lfirst(lc);
            Assert(IsA(rte, RangeTblEntry));
            if (rte == NULL) {
                continue;
            }
            char src[64] = {0};
            char dst[64] = {0};
            sprintf(src, "%s\n", rte->relname);
            sprintf(dst, "%s\n", tablename);
            ereport(LOG, (errmsg("rel: %s, %d, tn: %s, %d, cmp: %d", src, sizeof(src), dst, sizeof(dst), pg_strcasecmp(dst, src))));
            if (pg_strcasecmp(dst, src) == 0) {
               isLog = true;
               break; 
            }
        }
    }

    if (!isLog) {
        standard_ExecutorRun(queryDesc, direction, count);   
        return;
    } 

    TransactionId xid = GetCurrentTransactionId();
    StringInfo param_str = makeStringInfo();

    ParamListInfo params = queryDesc->params;
    if (params == NULL) {
        ereport(LOG, (errmsg("[full sql]ExecutorRun_hook: %s, Transaction ID: %u", queryDesc->sourceText, xid))); 
        standard_ExecutorRun(queryDesc, direction, count);
        return;        
    }

    for (int paramno = 0; paramno < params->numParams; paramno++) {
        ParamExternData* prm = &params->params[paramno];
        if (prm == NULL) {
            continue;
        }
        Oid typoutput;
        bool typisvarlena = false;
        char* pstring = NULL;

        appendStringInfo(param_str, "%s$%d = ", (paramno > 0) ? ", " : "", paramno + 1);

        if (prm->isnull || !OidIsValid(prm->ptype)) {
            appendStringInfoString(param_str, "NULL");
            continue;
        }
        getTypeOutputInfo(prm->ptype, &typoutput, &typisvarlena);
        pstring = OidOutputFunctionCall(typoutput, prm->value);

        appendStringInfoCharMacro(param_str, '\'');
        char* chunk_search_start = pstring;
        char* chunk_copy_start = pstring;
        char* chunk_end = NULL;
        while ((chunk_end = strchr(chunk_search_start, '\'')) != NULL) {
            appendBinaryStringInfoNT(param_str,
                                     chunk_copy_start,
                                     chunk_end - chunk_copy_start + 1);
            chunk_copy_start = chunk_end;
            chunk_search_start = chunk_end + 1;
        }
        appendStringInfo(param_str, "%s'", chunk_copy_start);
        pfree(pstring);
    }

    ereport(LOG, (errmsg("[full sql]ExecutorRun_hook: %s, Transaction ID: %u, paramlist: %s", queryDesc->sourceText, xid, param_str->data)));
    DestroyStringInfo(param_str);
    standard_ExecutorRun(queryDesc, direction, count);
}


void _PG_init(void);

void _PG_init(void) {


    ProcessUtility_old_hook = ProcessUtility_hook;
    ProcessUtility_hook = ogProcessUtility_hook;

    ExecutorRun_old_hook = ExecutorRun_hook;
    ExecutorRun_hook = ogExecutorRun_hook;

    DefineCustomStringVariable("full_sql.tablename",
                                "log which table's full sql", 
                                NULL, 
                                &tablename, 
                                "*",
                                PGC_POSTMASTER,
                                0, 
                                NULL, NULL, NULL);
}

PG_MODULE_MAGIC;