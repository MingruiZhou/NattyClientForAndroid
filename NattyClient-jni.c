/*
 *  Author : WangBoJing , email : 1989wangbojing@gmail.com
 *
 *  Copyright Statement:
 *  --------------------
 *  This software is protected by Copyright and the information contained
 *  herein is confidential. The software may not be copied and the information
 *  contained herein may not be used or disclosed except with the written
 *  permission of Author. (C) 2016
 *
 *

****       *****
  ***        *
  ***        *                         *               *
  * **       *                         *               *
  * **       *                         *               *
  *  **      *                        **              **
  *  **      *                       ***             ***
  *   **     *       ******       ***********     ***********    *****    *****
  *   **     *     **     **          **              **           **      **
  *    **    *    **       **         **              **           **      *
  *    **    *    **       **         **              **            *      *
  *     **   *    **       **         **              **            **     *
  *     **   *            ***         **              **             *    *
  *      **  *       ***** **         **              **             **   *
  *      **  *     ***     **         **              **             **   *
  *       ** *    **       **         **              **              *  *
  *       ** *   **        **         **              **              ** *
  *        ***   **        **         **              **               * *
  *        ***   **        **         **     *        **     *         **
  *         **   **        **  *      **     *        **     *         **
  *         **    **     ****  *       **   *          **   *          *
*****        *     ******   ***         ****            ****           *
                                                                       *
                                                                      *
                                                                  *****
                                                                  ****


 *
 */

#include <string.h>

#include "NattyClient-jni.h"
#include "NattyProtoClient.h"

#include <pthread.h>
#include <android/log.h>




JavaVM *gJavaVM;
jobject gJavaObj;

static volatile int gIsThreadExit = 0;


static void* native_thread_exec(void *arg){
	JNIEnv *env;
	(*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL);

	jclass javaClass = (*env)->GetObjectClass(env, gJavaObj);
	if (javaClass == NULL) {
		LOG("Fail to find javaClass");
		return 0;
	}

	jmethodID javaCallback = (*env)->GetMethodID(env, javaClass, "onNativeCallback", "(I)V");
	if (javaCallback == NULL) {
		LOG("Fail to find method onNativeCallback");
		return 0;
	}

	int count = 0;
	while (!gIsThreadExit) {
		(*env)->CallVoidMethod(env, gJavaObj, javaCallback, count++);
		sleep(2);
	}
	(*gJavaVM)->DetachCurrentThread(gJavaVM);

	LOG("native_thread_exec loop leave");
}

void Java_com_wbj_ndk_natty_client_NattyClient_nativeInitilize(JNIEnv *env, jobject thiz) {
	(*env)->GetJavaVM(env, &gJavaVM);
	gJavaObj = (*env)->NewGlobalRef(env, thiz);
}

void Java_com_wbj_ndk_natty_client_NattyClient_nativeThreadStart(JNIEnv *env, jobject thiz) {
	gIsThreadExit = 0;
	pthread_t threadId;
	if (pthread_create(&threadId, NULL, native_thread_exec, NULL) != 0) {
		LOG("native thread start failed !");
		return;
	}

	LOG("native thread start success !");
}

void Java_com_wbj_ndk_natty_client_NattyClient_nativeThreadStop(JNIEnv *env, jobject thiz) {
	gIsThreadExit = 1;
	LOG("native thread stop success !");
}

jstring Java_com_wbj_ndk_natty_client_NattyClient_stringFromJNI(JNIEnv *env, jobject thiz) {

#if defined(__arm__)
  #if defined(__ARM_ARCH_7A__)
    #if defined(__ARM_NEON__)
      #if defined(__ARM_PCS_VFP)
        #define ABI "armeabi-v7a/NEON (hard-float)"
      #else
        #define ABI "armeabi-v7a/NEON"
      #endif
    #else
      #if defined(__ARM_PCS_VFP)
        #define ABI "armeabi-v7a (hard-float)"
      #else
        #define ABI "armeabi-v7a"
      #endif
    #endif
  #else
   #define ABI "armeabi"
  #endif
#elif defined(__i386__)
   #define ABI "x86"
#elif defined(__x86_64__)
   #define ABI "x86_64"
#elif defined(__mips64)  /* mips64el-* toolchain defines __mips__ too */
   #define ABI "mips64"
#elif defined(__mips__)
   #define ABI "mips"
#elif defined(__aarch64__)
   #define ABI "arm64-v8a"
#else
   #define ABI "unknown"
#endif

    return (*env)->NewStringUTF(env, "Hello from JNI !  Compiled with ABI " ABI ".");//
}

jint Java_com_wbj_ndk_natty_client_NattyClient_add(JNIEnv *env, jobject thiz, jint a, jint b) {
	return a+b;
}


void ntyCallJavaFunction(const char *func, int arg) {
	JNIEnv *env;
	LOG("ntyCallJavaFunction");
	(*gJavaVM)->AttachCurrentThread(gJavaVM, &env, NULL);
	LOG("AttachCurrentThread");
	jclass javaClass = (*env)->GetObjectClass(env, gJavaObj);
	if (javaClass == NULL) {
		LOG("Fail to find javaClass");
		return ;
	}
	LOG("javaClass");
	jmethodID javaCallback = (*env)->GetMethodID(env, javaClass, func, "(I)V");
	if (javaCallback == NULL) {
		LOG("Fail to find method %s", func);
		return ;
	}
	LOG("javaCallback");
	(*env)->CallVoidMethod(env, gJavaObj, javaCallback, arg);

	(*gJavaVM)->DetachCurrentThread(gJavaVM);
	LOG("native_thread_exec loop leave");
}

void ntySendSuccess(int arg) {
	LOG("Success\n");
	ntyCallJavaFunction("onNativeSendSuccess", arg);
}

void ntySendFailed(int arg) {
	LOG("failed : %d\n", arg);
	ntyCallJavaFunction("onNativeSendFailed", arg);
}

void ntyUserRecvCb(int len) {
	LOG("ntyUserRecvCb : %d\n", len);
	ntyCallJavaFunction("onNativeUserRecvCallback", len);
}

int Java_com_wbj_ndk_natty_client_NattyClient_getBufferSize(JNIEnv *env, jobject thiz) {
	return ntyGetRecvBufferSize();
}

jbyteArray Java_com_wbj_ndk_natty_client_NattyClient_getNativeBuffer(JNIEnv *env, jobject thiz) {
	char *buffer = ntyGetRecvBuffer();
#if 1
	jsize len = ntyGetRecvBufferSize();

	jbyteArray rtnArr = (*env)->NewByteArray(env, len);
	(*env)->SetByteArrayRegion(env, rtnArr, 0, len, (jbyte*)buffer);
	return rtnArr;
#else
	return (*env)->NewStringUTF(env, buffer);
#endif
}

void Java_com_wbj_ndk_natty_client_NattyClient_setNattyDevId(JNIEnv *env, jobject thiz, jbyteArray DevIdArray) {
	jbyte *data = (*env)->GetByteArrayElements(env, DevIdArray, 0);

	C_DEVID AppId = *(C_DEVID*)data;
	ntySetDevId(AppId);

	LOG(" setNattyDevId : %lld ", AppId);

	return ;
}

void Java_com_wbj_ndk_natty_client_NattyClient_ntyClientInitilize(JNIEnv *env, jobject thiz) {
	(*env)->GetJavaVM(env, &gJavaVM);
	gJavaObj = (*env)->NewGlobalRef(env, thiz);

	ntySetProxyCallback(ntyUserRecvCb);
	ntySetSendFailedCallback(ntySendFailed);
	ntySetSendSuccessCallback(ntySendSuccess);
	LOG(" ntyClientInitilize ");
	return ;
}



