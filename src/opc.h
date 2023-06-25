#ifndef MISP_OPC_H
#define MISP_OPC_H

#define MISP_OPC_QUOTE 1
#define MISP_OPC_EQ 2
#define MISP_OPC_EQN 3
#define MISP_OPC_COND 5
#define MISP_OPC_LOOP 6
#define MISP_OPC_EVAL 7
/* #define MISP_OPC_TRAP 8 */
/* #define MISP_OPC_CLIMB 9 */
#define MISP_OPC_DO 10
#define MISP_OPC_LET 11
#define MISP_OPC_GET 12
#define MISP_OPC_SET 13

#define MISP_OPC_NADD 20
#define MISP_OPC_NSUB 21
#define MISP_OPC_NMUL 22
#define MISP_OPC_NDIV 23
#define MISP_OPC_NREM 24
#define MISP_OPC_NMOD 25
#define MISP_OPC_NAND 26
#define MISP_OPC_NOR 27
#define MISP_OPC_NXOR 28
#define MISP_OPC_NLSR 29
#define MISP_OPC_NGRT 30
#define MISP_OPC_NGRTEQ 31
#define MISP_OPC_NLSREQ 32

#define MISP_OPC_NNOT 35

#define MISP_OPC_LLEN 71
#define MISP_OPC_LGET 72
#define MISP_OPC_LSET 73
#define MISP_OPC_LSUB 74
#define MISP_OPC_LINT 75

#define MISP_OPC_DBUG 67

#endif
