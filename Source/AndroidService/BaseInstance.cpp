// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseInstance.h"
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include <android/log.h>
#endif

void UBaseInstance::ShowToast(const FString& Content)
{
#if PLATFORM_ANDROID
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        jstring JavaString = Env->NewStringUTF(TCHAR_TO_UTF8(*Content));
        static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_AndroidAPI_ShowToast", "(Ljava/lang/String;)V", false);
        FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, JavaString);
    }
#endif
}