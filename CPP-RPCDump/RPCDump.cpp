/*
 * Copyright (c) BindView Development Corporation, 2001
 * See LICENSE file.
 * Author: Todd Sabin <tsabin@razor.bindview.com>
 */


#include <windows.h>
#include <winnt.h>

#include <stdio.h>

#include <rpc.h>
#include <rpcdce.h>

static int verbosity = 0;


int try_protocol(RPC_WSTR server, RPC_WSTR protocol)
{
    RPC_WSTR szStringBinding = NULL;
    RPC_BINDING_HANDLE hRpc;
    RPC_EP_INQ_HANDLE hInq;
    RPC_STATUS rpcErr;
    RPC_STATUS rpcErr2;
    int numFound = 0;

    //
    // Compose the string binding
    //
    rpcErr = RpcStringBindingCompose(
        NULL,
        protocol,
        server,
        NULL,
        NULL,
        &szStringBinding
    );
    if (rpcErr != RPC_S_OK) {
        fprintf(stderr, "RpcStringBindingCompose failed: %d\n", rpcErr);
        return numFound;
    }

    //
    // Convert to real binding
    //
    rpcErr = RpcBindingFromStringBinding(szStringBinding, &hRpc);
    if (rpcErr != RPC_S_OK) {
        fprintf(stderr, "RpcBindingFromStringBinding failed: %d\n", rpcErr);
        RpcStringFree(&szStringBinding);
        return numFound;
    }

    //
    // Begin Ep enum
    //
    rpcErr = RpcMgmtEpEltInqBegin(
        hRpc,
        RPC_C_EP_ALL_ELTS,  // _In_ InquiryType: Get every element from the endpoint map.  
        NULL,               // _In_ IfId: not needed since we want every element from emapper
        NULL,               // _In_ VersOption: not  needed since we want every element from emapper
        NULL,               // _In_ ObjectUuid: not needed since InquiryType is RPC_C_EP_ALL_ELTS
        &hInq
    );
    if (rpcErr != RPC_S_OK) {
        fprintf(stderr, "RpcMgmtEpEltInqBegin failed: %d\n", rpcErr);
        RpcStringFree(&szStringBinding);
        //RpcBindingFree(&hRpc);
        return numFound;
    }

    //
    // While Next succeeds
    //
    do {
        RPC_IF_ID IfId;
        RPC_IF_ID_VECTOR* pVector;
        RPC_STATS_VECTOR* pStats;
        RPC_BINDING_HANDLE hEnumBind;
        UUID uuid;
        RPC_WSTR szAnnot;

        rpcErr = RpcMgmtEpEltInqNext(
            hInq,           // _In_ InquiryContext
            &IfId,          // _Out_ IfId
            &hEnumBind,     // _Out_opt Binding: Returns binding handle from the endpoint-map element
            &uuid,          // _Out_opt ObjectUuid: Returns the object UUID from the endpoint-map element.
            &szAnnot        // _Out_opt Annotation: Returns the annotation string for the endpoint-map element. 
        );
        if (rpcErr == RPC_S_OK) {
            RPC_WSTR str = NULL;
            RPC_WSTR princName = NULL;

            // increment numFound
            numFound++;

            //
            // Print IfId
            //
            if (UuidToString(&(IfId.Uuid), &str) == RPC_S_OK) {
                wprintf(L"IfId: %s version %d.%d\n", str, IfId.VersMajor,
                    IfId.VersMinor);
                RpcStringFree(&str);
            }

            //
            // Print Annot
            //
            if (szAnnot) {
                wprintf(L"Annotation: %s\n", szAnnot);
                RpcStringFree(&szAnnot);
            }

            //
            // Print object ID
            //
            if (UuidToString(&uuid, &str) == RPC_S_OK) {
                wprintf(L"UUID: %s\n", str);
                RpcStringFree(&str);
            }

            //
            // Print Binding
            //
            if (RpcBindingToStringBinding(hEnumBind, &str) == RPC_S_OK) {
                wprintf(L"Binding: %s\n", str);
                RpcStringFree(&str);
            }

            if (verbosity >= 1) {
                RPC_WSTR strBinding = NULL;
                RPC_WSTR strObj = NULL;
                RPC_WSTR strProtseq = NULL;
                RPC_WSTR strNetaddr = NULL;
                RPC_WSTR strEndpoint = NULL;
                RPC_WSTR strNetoptions = NULL;
                RPC_BINDING_HANDLE hIfidsBind;

                //
                // Ask the RPC server for its supported interfaces
                //
                //
                // Because some of the binding handles may refer to
                // the machine name, or a NAT'd address that we may
                // not be able to resolve/reach, parse the binding and
                // replace the network address with the one specified
                // from the command line.  Unfortunately, this won't
                // work for ncacn_nb_tcp bindings because the actual
                // NetBIOS name is required.  So special case those.
                //
                // Also, skip ncalrpc bindings, as they are not
                // reachable from a remote machine.
                //
                rpcErr2 = RpcBindingToStringBinding(hEnumBind, &strBinding);
                RpcBindingFree(&hEnumBind);
                if (rpcErr2 != RPC_S_OK) {
                    fprintf(stderr, ("RpcBindingToStringBinding failed\n"));
                    printf("\n");
                    continue;
                }

                //strBinding.;
                if (wcsstr((LPCWSTR)strBinding, L"ncalrpc") != NULL) {
                    RpcStringFree(&strBinding);
                    printf("\n");
                    continue;
                }

                rpcErr2 = RpcStringBindingParse(
                    strBinding,
                    &strObj,
                    &strProtseq,
                    &strNetaddr,
                    &strEndpoint,
                    &strNetoptions
                );
                RpcStringFree(&strBinding);
                strBinding = NULL;
                if (rpcErr2 != RPC_S_OK) {
                    fprintf(stderr, ("RpcStringBindingParse failed\n"));
                    printf("\n");
                    continue;
                }

                rpcErr2 = RpcStringBindingCompose(
                    strObj,
                    strProtseq,
                    wcscmp(L"ncacn_nb_tcp", (LPCWSTR)strProtseq) == 0 ? strNetaddr : server,
                    strEndpoint, strNetoptions,
                    &strBinding
                );
                RpcStringFree(&strObj);
                RpcStringFree(&strProtseq);
                RpcStringFree(&strNetaddr);
                RpcStringFree(&strEndpoint);
                RpcStringFree(&strNetoptions);
                if (rpcErr2 != RPC_S_OK) {
                    fprintf(stderr, ("RpcStringBindingCompose failed\n"));
                    printf("\n");
                    continue;
                }

                rpcErr2 = RpcBindingFromStringBinding(strBinding, &hIfidsBind);
                RpcStringFree(&strBinding);
                if (rpcErr2 != RPC_S_OK) {
                    fprintf(stderr, ("RpcBindingFromStringBinding failed\n"));
                    printf("\n");
                    continue;
                }

                if ((rpcErr2 = RpcMgmtInqIfIds(hIfidsBind, &pVector)) == RPC_S_OK) {
                    unsigned int i;
                    wprintf(L"RpcMgmtInqIfIds succeeded\n");
                    wprintf(L"Interfaces: %d\n", pVector->Count);
                    for (i = 0; i < pVector->Count; i++) {
                        RPC_WSTR str = NULL;
                        UuidToString(&pVector->IfId[i]->Uuid, &str);
                        wprintf(L"  %s v%d.%d\n", str ? str : (RPC_WSTR)L"(null)",
                            pVector->IfId[i]->VersMajor,
                            pVector->IfId[i]->VersMinor);
                        if (str) RpcStringFree(&str);
                    }
                    RpcIfIdVectorFree(&pVector);
                }
                else {
                    wprintf(L"RpcMgmtInqIfIds failed: 0x%x\n", rpcErr2);
                }

                //if (verbosity >= 2) { // No extra verbosity check -v should be enough
                if ((rpcErr2 = RpcMgmtInqServerPrincName(
                    hEnumBind,
                    RPC_C_AUTHN_WINNT,
                    &princName
                )) == RPC_S_OK) {
                    wprintf(L"RpcMgmtInqServerPrincName succeeded\n");
                    wprintf(L"Name: %s\n", princName);
                    RpcStringFree(&princName);
                }
                else {
                    wprintf(L"RpcMgmtInqServerPrincName failed: 0x%x\n", rpcErr2);
                }

                if ((rpcErr2 = RpcMgmtInqStats(
                    hEnumBind,
                    &pStats
                )) == RPC_S_OK) {
                    unsigned int i;
                    wprintf(L"RpcMgmtInqStats succeeded\n");
                    for (i = 0; i < pStats->Count; i++) {
                        wprintf(L"  Stats[%d]: %d\n", i, pStats->Stats[i]);
                    }
                    RpcMgmtStatsVectorFree(&pStats);
                }
                else {
                    wprintf(L"RpcMgmtInqStats failed: 0x%x\n", rpcErr2);
                }
                //}
                RpcBindingFree(&hIfidsBind);
            }
            wprintf(L"\n");
        }
    } while (rpcErr != RPC_X_NO_MORE_ENTRIES);

    //
    // Done
    //
    RpcStringFree(&szStringBinding);
    RpcBindingFree(&hRpc);

    return numFound;
}


RPC_WSTR protocols[] = {
    (RPC_WSTR)L"ncacn_ip_tcp",
    (RPC_WSTR)L"ncadg_ip_udp",
    (RPC_WSTR)L"ncacn_np",
    (RPC_WSTR)L"ncacn_nb_tcp",
    (RPC_WSTR)L"ncacn_http",
};
#define NUM_PROTOCOLS (sizeof (protocols) / sizeof (protocols[0]))

void
Usage(wchar_t* app)
{
    printf("Usage: %s [options] <target>\n", app);
    printf("  options:\n");
    printf("    -v           -- increase verbosity\n", app);
    exit(1);
}



int
wmain(int argc, wchar_t* argv[], wchar_t* envp[])
{
    int i, j;
    RPC_WSTR target = NULL;
    RPC_WSTR protseq = NULL;
    int nRPCInt = 0;
    for (j = 1; j < argc; j++) {
        if (argv[j][0] == '-') {
            switch (argv[j][1]) {

            case 'v':
                verbosity++;
                break;

            default:
                Usage(argv[0]);
                break;
            }
        }
        else {
            target = (RPC_WSTR)argv[j];
        }
    }

    if (!target) {
        wprintf(L"[!] Usage: %s <server>\n", argv[0]);
        exit(1);
    }

    for (i = 0; i < NUM_PROTOCOLS; i++) {
        protseq = protocols[i];
        wprintf(L"## Testing protseq.: %s\n\n", protocols[i]);
        nRPCInt += try_protocol(target, protseq);
    }
    wprintf(L"[*] Found %d RPC Interfaces at '%s' (Verbosity: %d)\n", nRPCInt, target, verbosity);
    
    
    return 0;
}
