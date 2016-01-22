#include "postgres.h"
#include "libpq-fe.h"
#include "miscadmin.h"

#include "connection.h"
#include "distributed_transaction_manager.h"

int PgShardCurrTransManager;

static bool PgShardBeginStub(PGconn *conn);
static bool PgShardPrepareStub(PGconn *conn);
static bool PgShardCommitPreparedStub(PGconn *conn);
static bool PgShardRollbackPreparedStub(PGconn *conn);
static bool PgShardRollbackStub(PGconn *conn);

static bool PgShardBegin1PC(PGconn *conn);
static bool PgShardPrepare1PC(PGconn *conn);
static bool PgShardCommitPrepared1PC(PGconn *conn);
static bool PgShardRollbackPrepared1PC(PGconn *conn);
static bool PgShardRollback1PC(PGconn *conn);

static bool PgShardBegin2PC(PGconn *conn);
static bool PgShardPrepare2PC(PGconn *conn);
static bool PgShardCommitPrepared2PC(PGconn *conn);
static bool PgShardRollbackPrepared2PC(PGconn *conn);
static bool PgShardRollback2PC(PGconn *conn);

static int GlobalTransactionId = 0;

PgShardTransactionManager const PgShardTransManagerImpl[] =
{
	{ PgShardBeginStub, PgShardPrepareStub, PgShardCommitPreparedStub,
	  PgShardRollbackPreparedStub, PgShardRollbackStub },
	{ PgShardBegin1PC, PgShardPrepare1PC, PgShardCommitPrepared1PC,
	  PgShardRollbackPrepared1PC, PgShardRollback1PC },
	{ PgShardBegin2PC, PgShardPrepare2PC, PgShardCommitPrepared2PC,
	  PgShardRollbackPrepared2PC, PgShardRollback2PC }
};

/*
 * Transaction manager stub
 */
static bool
PgShardBeginStub(PGconn *conn)
{
	return true;
}


static bool
PgShardPrepareStub(PGconn *conn)
{
	return true;
}


static bool
PgShardCommitPreparedStub(PGconn *conn)
{
	return true;
}


static bool
PgShardRollbackPreparedStub(PGconn *conn)
{
	return true;
}


static bool
PgShardRollbackStub(PGconn *conn)
{
	return true;
}


/*
 * One-phase commit
 */

static bool
PgShardBegin1PC(PGconn *conn)
{
	return PgShardExecute(conn, PGRES_COMMAND_OK, "BEGIN TRANSACTION");
}


static bool
PgShardPrepare1PC(PGconn *conn)
{
	return true;
}


static bool
PgShardCommitPrepared1PC(PGconn *conn)
{
	return PgShardExecute(conn, PGRES_COMMAND_OK, "COMMIT");
}


static bool
PgShardRollbackPrepared1PC(PGconn *conn)
{
	return PgShardExecute(conn, PGRES_COMMAND_OK, "ROLLBACK");
}


static bool
PgShardRollback1PC(PGconn *conn)
{
	return PgShardExecute(conn, PGRES_COMMAND_OK, "ROLLBACK");
}

/*
 * Two-phase commit
 */
static char *
PgShard2pcCommand(char const *cmd)
{
	return psprintf("%s 'pgshard_%d_%d'", cmd, MyProcPid, GlobalTransactionId);
}


static bool
PgShardBegin2PC(PGconn *conn)
{
	return PgShardExecute(conn, PGRES_COMMAND_OK, "BEGIN TRANSACTION");
}


static bool
PgShardPrepare2PC(PGconn *conn)
{
	GlobalTransactionId += 1;
	return PgShardExecute(conn, PGRES_COMMAND_OK, PgShard2pcCommand(
							  "PREPARE TRANSACTION"));
}


static bool
PgShardCommitPrepared2PC(PGconn *conn)
{
	return PgShardExecute(conn, PGRES_COMMAND_OK, PgShard2pcCommand("COMMIT PREPARED"));
}


static bool
PgShardRollbackPrepared2PC(PGconn *conn)
{
	return PgShardExecute(conn, PGRES_COMMAND_OK, PgShard2pcCommand("ROLLBACK PREPARED"));
}


static bool
PgShardRollback2PC(PGconn *conn)
{
	return PgShardExecute(conn, PGRES_COMMAND_OK, "ROLLBACK");
}


/*
 * Execute statement with specified parameters and check its result
 */
bool
PgShardExecute(PGconn *conn, ExecStatusType expectedResult, char const *sql)
{
	bool ret = true;
	PGresult *result = PQexec(conn, sql);
	if (PQresultStatus(result) != expectedResult)
	{
		ReportRemoteError(conn, result);
		ret = false;
	}
	PQclear(result);
	return ret;
}