/*
 * Copyright (C) 2010-2013 Volodymyr Tarasenko <tvntsr@yahoo.com>
 *               2010      Sergey Pavlov <sergey.pavlov@gmail.com>
 *               2010      PortaOne Inc.
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and assOCIated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OracleAdapter.h"
#include "StringBuffer.h"


/**
 * Implementation of the ResulSet/Delegate interface for oracle.
 *
 * @file
 */


/* ----------------------------------------------------------- Definitions */


typedef struct column_t {
        OCIDefine *def;
        int isNull;
        char *buffer;
        char *name;
        unsigned long length;
        OCILobLocator *lob_loc;
        OCIDateTime   *date;
} *column_t;
#define T ResultSetDelegate_T
struct T {
        int         columnCount;
        int         currentRow;
        int         fetchSize;
        ub4         maxRows;
        OCIStmt*    stmt;
        OCIEnv*     env;
        OCISession* usr;
        OCIError*   err;
        OCISvcCtx*  svc;
        column_t    columns;
        sword       lastError;
        int         freeStatement;
        Connection_T delegator;
};
#ifndef ORACLE_COLUMN_NAME_LOWERCASE
#define ORACLE_COLUMN_NAME_LOWERCASE 1
#endif
#define LOB_CHUNK_SIZE  2000
#define DATE_STR_BUF_SIZE   255


/* ------------------------------------------------------- Private methods */


static bool _initaleDefiningBuffers(T R) {
        ub2 dtype = 0;
        int deptlen;
        int sizelen = sizeof(deptlen);
        OCIParam* pard = NULL;
        sword status;
        for (int i = 1; i <= R->columnCount; i++) {
                deptlen = 0;
                /* The next two statements describe the select-list item, dname, and
                 return its length */
                R->lastError = OCIParamGet(R->stmt, OCI_HTYPE_STMT, R->err, (void **)&pard, i);
                if (R->lastError != OCI_SUCCESS)
                        return false;
                R->lastError = OCIAttrGet(pard, OCI_DTYPE_PARAM, &deptlen, &sizelen, OCI_ATTR_DATA_SIZE, R->err);
                if (R->lastError != OCI_SUCCESS) {
                        // cannot get column's size, cleaning and returning
                        OCIDescriptorFree(pard, OCI_DTYPE_PARAM);
                        return false;
                }
                OCIAttrGet(pard, OCI_DTYPE_PARAM, &dtype, 0, OCI_ATTR_DATA_TYPE, R->err);
                /* Use the retrieved length of dname to allocate an output buffer, and
                 then define the output variable. */
                deptlen +=1;
                // printf("查询deptlen:%d,查询dtype:%d\n",deptlen,dtype);
                R->columns[i-1].length = deptlen;
                R->columns[i-1].isNull = 0;
                switch(dtype)
                {
                        case SQLT_BLOB:
                                // printf("查询blob");
                                R->columns[i-1].buffer = NULL;
                                status = OCIDescriptorAlloc((dvoid *)R->env, (dvoid **) &(R->columns[i-1].lob_loc),
                                                            (ub4) OCI_DTYPE_LOB,
                                                            (size_t) 0, (dvoid **) 0);
                                R->lastError = OCIDefineByPos(R->stmt, &R->columns[i-1].def, R->err, i,
                                                              &(R->columns[i-1].lob_loc), deptlen, SQLT_BLOB, &(R->columns[i-1].isNull), 0, 0, OCI_DEFAULT);
                                // printf("查看buffer的值:%s\n",R->columns[i-1].buffer);
                                break;
                                
                        case SQLT_CLOB:
                                // printf("查询clob");
                                R->columns[i-1].buffer = NULL;
                                status = OCIDescriptorAlloc((dvoid *)R->env, (dvoid **) &(R->columns[i-1].lob_loc),
                                                            (ub4) OCI_DTYPE_LOB,
                                                            (size_t) 0, (dvoid **) 0);
                                R->lastError = OCIDefineByPos(R->stmt, &R->columns[i-1].def, R->err, i,
                                                              &(R->columns[i-1].lob_loc), deptlen, SQLT_CLOB, &(R->columns[i-1].isNull), 0, 0, OCI_DEFAULT);
                                // printf("查看buffer的值:%s\n",R->columns[i-1].buffer);
                                break;
                        case SQLT_DAT:
                        case SQLT_DATE:
                        case SQLT_TIMESTAMP:
                        case SQLT_TIMESTAMP_TZ:
                        case SQLT_TIMESTAMP_LTZ:
                                // printf("SQLT_TIMESTAMP_LTZ\n");
                                R->columns[i-1].buffer = NULL;
                                status = OCIDescriptorAlloc((dvoid *)R->env, (dvoid **) &(R->columns[i-1].date),
                                                            (ub4) OCI_DTYPE_TIMESTAMP,
                                                            (size_t) 0, (dvoid **) 0);
                                R->lastError = OCIDefineByPos(R->stmt, &R->columns[i-1].def, R->err, i,
                                                              &(R->columns[i-1].date), sizeof(R->columns[i-1].date), SQLT_TIMESTAMP, &(R->columns[i-1].isNull), 0, 0, OCI_DEFAULT);
                                break;
                        default:
                                // printf("default\n");
                                R->columns[i-1].lob_loc = NULL;
                                R->columns[i-1].buffer = ALLOC(2*deptlen + 1);
                                R->lastError = OCIDefineByPos(R->stmt, &R->columns[i-1].def, R->err, i,
                                                              R->columns[i-1].buffer, (2*deptlen+1), SQLT_STR, &(R->columns[i-1].isNull), 0, 0, OCI_DEFAULT);
                                // printf("查看buffer的值:%s\n",R->columns[i-1].buffer);
                }
                {
                        char *col_name;
                        ub4   col_name_len;
                        char* tmp_buffer;
                        
                        R->lastError = OCIAttrGet(pard, OCI_DTYPE_PARAM, &col_name, &col_name_len, OCI_ATTR_NAME, R->err);
                        if (R->lastError != OCI_SUCCESS)
                                continue;
                        // printf("查看col_name：%s\n",col_name);
                        // column name could be non NULL terminated
                        // it is not allowed to do: col_name[col_name_len] = 0;
                        // so, copy the string
                        tmp_buffer = Str_ndup(col_name, col_name_len);
                        // printf("查看buffer:%s\n",tmp_buffer);
#if defined(ORACLE_COLUMN_NAME_LOWERCASE) && ORACLE_COLUMN_NAME_LOWERCASE > 1
                        R->columns[i-1].name = CALLOC(1, col_name_len+1);
                        OCIMultiByteStrCaseConversion(R->env, R->columns[i-1].name, tmp_buffer, OCI_NLS_LOWERCASE);
                        FREE(tmp_buffer);
#else
                        R->columns[i-1].name = tmp_buffer;
#endif /*COLLUMN_NAME_LOWERCASE*/
                }
                OCIDescriptorFree(pard, OCI_DTYPE_PARAM);
                if (R->lastError != OCI_SUCCESS) {
                        return false;
                }
        }
        return true;
}

static bool _toString(T R, int i)
{
        const char fmt[] = "IYYY-MM-DD HH24.MI.SS"; // "YYYY-MM-DD HH24:MI:SS TZR TZD"
        
        R->columns[i].length = DATE_STR_BUF_SIZE;
        if (R->columns[i].buffer)
                FREE(R->columns[i].buffer);
        
        R->columns[i].buffer = ALLOC(R->columns[i].length + 1);
        R->lastError = OCIDateTimeToText(R->usr,
                                         R->err,
                                         R->columns[i].date,
                                         fmt, strlen(fmt),
                                         0,
                                         NULL, 0,
                                         (ub4*)&(R->columns[i].length), (dutext *)R->columns[i].buffer);
        return ((R->lastError == OCI_SUCCESS) || (R->lastError == OCI_SUCCESS_WITH_INFO));;
}

static void _setFetchSize(T R, int rows);


/* ------------------------------------------------------------- Constructor */


T OracleResultSet_new(Connection_T delegator, OCIStmt *stmt, OCIEnv *env, OCISession* usr, OCIError *err, OCISvcCtx *svc, int need_free) {
        T R;
        assert(stmt);
        assert(env);
        assert(err);
        assert(svc);
        NEW(R);
        R->delegator = delegator;
        R->stmt = stmt;
        R->env  = env;
        R->err  = err;
        R->svc  = svc;
        R->usr  = usr;
        R->maxRows = Connection_getMaxRows(R->delegator);
        R->freeStatement = need_free;
        /* Get the number of columns in the select list */
        R->lastError = OCIAttrGet (R->stmt, OCI_HTYPE_STMT, &R->columnCount, NULL, OCI_ATTR_PARAM_COUNT, R->err);
        // printf("%d\n",R->columnCount);
        if (R->lastError != OCI_SUCCESS && R->lastError != OCI_SUCCESS_WITH_INFO)
                DEBUG("_new: Error %d, '%s'\n", R->lastError, OraclePreparedStatement_getLastError(R->lastError,R->err));
        R->columns = CALLOC(R->columnCount, sizeof (struct column_t));
        if (!_initaleDefiningBuffers(R)) {
                DEBUG("_new: Error %d, '%s'\n", R->lastError, OraclePreparedStatement_getLastError(R->lastError,R->err));
                R->currentRow = -1;
        }
        if (R->currentRow != -1) {
                _setFetchSize(R, Connection_getFetchSize(R->delegator));
        }
        return R;
}


/* -------------------------------------------------------- Delegate Methods */


static void _free(T *R) {
        assert(R && *R);
        if ((*R)->freeStatement)
                OCIHandleFree((*R)->stmt, OCI_HTYPE_STMT);
        for (int i = 0; i < (*R)->columnCount; i++) {
                if ((*R)->columns[i].lob_loc)
                        OCIDescriptorFree((*R)->columns[i].lob_loc, OCI_DTYPE_LOB);
                if ((*R)->columns[i].date)
                        OCIDescriptorFree((dvoid*)(*R)->columns[i].date, OCI_DTYPE_TIMESTAMP);
                FREE((*R)->columns[i].buffer);
                FREE((*R)->columns[i].name);
        }
        FREE((*R)->columns);
        FREE(*R);
}


static int _getColumnCount(T R) {
        assert(R);
        return R->columnCount;
}


static const char *_getColumnName(T R, int column) {
        assert(R);
        if (R->columnCount < column)
                return NULL;
        return  R->columns[column-1].name;
}


static long _getColumnSize(T R, int columnIndex) {
        OCIParam* pard = NULL;
        ub4 char_semantics = 0;
        sb4 status;
        ub2 col_width = 0;
        assert(R);
        status = OCIParamGet(R->stmt, OCI_HTYPE_STMT, R->err, (void **)&pard, columnIndex);
        if (status != OCI_SUCCESS)
                return -1;
        status = OCIAttrGet(pard, OCI_DTYPE_PARAM, &char_semantics, NULL, OCI_ATTR_CHAR_USED, R->err);
        if (status != OCI_SUCCESS) {
                OCIDescriptorFree(pard, OCI_DTYPE_PARAM);
                return -1;
        }
        status = (char_semantics) ?
        /* Retrieve the column width in characters */
        OCIAttrGet(pard, OCI_DTYPE_PARAM, &col_width, NULL, OCI_ATTR_CHAR_SIZE, R->err) :
        /* Retrieve the column width in bytes */
        OCIAttrGet(pard, OCI_DTYPE_PARAM, &col_width, NULL, OCI_ATTR_DATA_SIZE, R->err);
        return (status != OCI_SUCCESS) ? -1 : col_width;
}


static void _setFetchSize(T R, int rows) {
        assert(R);
        assert(rows > 0);
        // printf("_setFetchSize查看rows:%d",rows);
        R->lastError = OCIAttrSet(R->stmt, OCI_HTYPE_STMT, (void*)&rows, (ub4)sizeof(ub4), OCI_ATTR_PREFETCH_ROWS, R->err);
        if (R->lastError != OCI_SUCCESS)
                DEBUG("OCIAttrSet -- %s\n", OraclePreparedStatement_getLastError(R->lastError, R->err));
        R->fetchSize = rows;
}


static int _getFetchSize(T R) {
        assert(R);
        return R->fetchSize;
}


static bool _next(T R) {
        // printf("nextnext\n");
        // printf("currentrow:%d\n",R->currentRow);
        assert(R);
        if ((R->currentRow < 0) || ((R->maxRows > 0) && (R->currentRow >= R->maxRows)))
        {
                // printf("1\n");
                return false;
        }       
        R->lastError = OCIStmtFetch(R->stmt, R->err, 1, OCI_FETCH_NEXT, OCI_DEFAULT);
        if (R->lastError == OCI_NO_DATA)
        {
                
                // printf("2\n");
                return false;
        }
        // printf("lastError:%d\n",R->lastError);   
        if (R->lastError != OCI_SUCCESS && R->lastError != OCI_SUCCESS_WITH_INFO)
        {
                // printf("3\n");
                THROW(SQLException, "%s", OraclePreparedStatement_getLastError(R->lastError, R->err));
        }       
        if (R->lastError == OCI_SUCCESS_WITH_INFO)
        {
                // printf("4\n");
                DEBUG("_next Error %d, '%s'\n", R->lastError, OraclePreparedStatement_getLastError(R->lastError, R->err));
        } 
        R->currentRow++;
        // printf("5\n");
        return ((R->lastError == OCI_SUCCESS) || (R->lastError == OCI_SUCCESS_WITH_INFO));
}


static bool _isnull(T R, int columnIndex) {
        assert(R);
        int i = checkAndSetColumnIndex(columnIndex, R->columnCount);
        return R->columns[i].isNull != 0;
}


static const char *_getString(T R, int columnIndex) {
        assert(R);
        int i = checkAndSetColumnIndex(columnIndex, R->columnCount);
        if (R->columns[i].isNull)
                return NULL;
        if (R->columns[i].date) {
                if (!_toString(R, i)) {
                        THROW(SQLException, "%s", OraclePreparedStatement_getLastError(R->lastError, R->err));
                }
        }
        if (R->columns[i].buffer)
                R->columns[i].buffer[R->columns[i].length] = 0;
        printf("查看得到的string值:%s\n",R->columns[i].buffer);
        return R->columns[i].buffer;
}


static const void *_getBlob(T R, int columnIndex, int *size) {
        assert(R);
        // printf("使用getblob方法\n");
        // printf("==========%d查看索引值\n",columnIndex);
        int i = checkAndSetColumnIndex(columnIndex, R->columnCount);
        if (R->columns[i].isNull)
                return NULL;
        if (R->columns[i].buffer)
                FREE(R->columns[i].buffer);
        oraub8 read_chars = 0;
        oraub8 read_bytes = 0;
        oraub8 total_bytes = 0;
        R->columns[i].buffer = ALLOC(LOB_CHUNK_SIZE);
        *size = 0;
        ub1 piece = OCI_FIRST_PIECE;
        // ub1 piece=;
        do {
                read_bytes = 0;
                read_chars = 0;
                // SQLCS_IMPLICIT
                R->lastError = OCILobRead2(R->svc, R->err, R->columns[i].lob_loc, &read_bytes, &read_chars, 1,
                                           R->columns[i].buffer + total_bytes, LOB_CHUNK_SIZE, piece, NULL, NULL, NULL, NULL);
                if (read_bytes) {
                        total_bytes += read_bytes;
                        piece = OCI_NEXT_PIECE;
                     
                        R->columns[i].buffer = RESIZE(R->columns[i].buffer, (long)(total_bytes + LOB_CHUNK_SIZE));
                }
        } while (R->lastError == OCI_NEED_DATA);
        if (R->lastError != OCI_SUCCESS && R->lastError != OCI_SUCCESS_WITH_INFO) {
                FREE(R->columns[i].buffer);
                R->columns[i].buffer = NULL;
                THROW(SQLException, "%s", OraclePreparedStatement_getLastError(R->lastError, R->err));
        }
        *size = R->columns[i].length = (int)total_bytes;
       // printf("-------------------%d,%d\n",R->columns[i].length,R->columns[i].name);
        return (const void *)R->columns[i].buffer;
}


/* ------------------------------------------------------------------------- */


const struct Rop_T oraclerops = {
        .name           = "oracle",
        .free           = _free,
        .getColumnCount = _getColumnCount,
        .getColumnName  = _getColumnName,
        .getColumnSize  = _getColumnSize,
        .setFetchSize   = _setFetchSize,
        .getFetchSize   = _getFetchSize,
        .next           = _next,
        .isnull         = _isnull,
        .getString      = _getString,
        .getBlob        = _getBlob
        // getTimestamp and getDateTime is handled in ResultSet
};

