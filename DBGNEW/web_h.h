

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.00.0603 */
/* at Sun Sep 28 18:35:13 2014
 */
/* Compiler settings for .\web.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.00.0603 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __web_h_h__
#define __web_h_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __ICppFromJS_FWD_DEFINED__
#define __ICppFromJS_FWD_DEFINED__
typedef interface ICppFromJS ICppFromJS;

#endif 	/* __ICppFromJS_FWD_DEFINED__ */


#ifndef __CppFromJS_FWD_DEFINED__
#define __CppFromJS_FWD_DEFINED__

#ifdef __cplusplus
typedef class CppFromJS CppFromJS;
#else
typedef struct CppFromJS CppFromJS;
#endif /* __cplusplus */

#endif 	/* __CppFromJS_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "mshtml.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_web_0000_0000 */
/* [local] */ 

#pragma once
#pragma region Desktop Family
#pragma endregion


extern RPC_IF_HANDLE __MIDL_itf_web_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_web_0000_0000_v0_0_s_ifspec;

#ifndef __ICppFromJS_INTERFACE_DEFINED__
#define __ICppFromJS_INTERFACE_DEFINED__

/* interface ICppFromJS */
/* [unique][nonextensible][uuid][object] */ 


EXTERN_C const IID IID_ICppFromJS;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("33333333-3333-3333-3333-333333333333")
    ICppFromJS
    {
    public:
        BEGIN_INTERFACE
        virtual /* [local] */ HRESULT STDMETHODCALLTYPE TestCall( 
            /* [in] */ BSTR lpText,
            /* [in] */ BSTR lpCaption,
            /* [in] */ unsigned int uType,
            /* [retval][out] */ unsigned int *pret) = 0;
        
        END_INTERFACE
    };
    
    
#else 	/* C style interface */

    typedef struct ICppFromJSVtbl
    {
        BEGIN_INTERFACE
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *TestCall )( 
            ICppFromJS * This,
            /* [in] */ BSTR lpText,
            /* [in] */ BSTR lpCaption,
            /* [in] */ unsigned int uType,
            /* [retval][out] */ unsigned int *pret);
        
        END_INTERFACE
    } ICppFromJSVtbl;

    interface ICppFromJS
    {
        CONST_VTBL struct ICppFromJSVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ICppFromJS_TestCall(This,lpText,lpCaption,uType,pret)	\
    ( (This)->lpVtbl -> TestCall(This,lpText,lpCaption,uType,pret) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ICppFromJS_INTERFACE_DEFINED__ */



#ifndef __XXXLib_LIBRARY_DEFINED__
#define __XXXLib_LIBRARY_DEFINED__

/* library XXXLib */
/* [version][uuid] */ 


EXTERN_C const IID LIBID_XXXLib;

EXTERN_C const CLSID CLSID_CppFromJS;

#ifdef __cplusplus

class DECLSPEC_UUID("33333333-3333-3333-3333-333333333331")
CppFromJS;
#endif
#endif /* __XXXLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


