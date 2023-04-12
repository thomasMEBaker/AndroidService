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
        static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidAPI_ShowToast", "(Ljava/lang/String;)V", false);
        FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method, JavaString);
    }
#endif
}

void UBaseInstance::InitaliseWebRTC()
{
#if PLATFORM_ANDROID
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidAPI_InitService", "()V", false);
        FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
    }
#endif
}

void UBaseInstance::DestroyWebRTC()
{
#if PLATFORM_ANDROID
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidAPI_DestroyService", "()V", false);
        FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
    }
#endif
}

FString UBaseInstance::GetDeviceIpAddress() {
	FString ipAddress = "unknown";
	#if PLATFORM_ANDROID	
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv()) {
		// Get the current activity's context
		jobject activity = FAndroidApplication::GetGameActivityThis();

		// Find the NetworkInterface class and the getNetworkInterfaces method
		jclass networkInterfaceClass = Env->FindClass("java/net/NetworkInterface");
		jmethodID getNetworkInterfacesMethod = Env->GetStaticMethodID(networkInterfaceClass, "getNetworkInterfaces", "()Ljava/util/Enumeration;");

		// Call the getNetworkInterfaces method to get the network interfaces
		jobject networkInterfacesObject = Env->CallStaticObjectMethod(networkInterfaceClass, getNetworkInterfacesMethod);

		jclass inetAddressClass = Env->FindClass("java/net/InetAddress");
		jmethodID isSiteLocalAddressMethod = Env->GetMethodID(inetAddressClass, "isSiteLocalAddress", "()Z");
		jmethodID isLoopbackAddressMethod = Env->GetMethodID(inetAddressClass, "isLoopbackAddress", "()Z");

		// Find the Enumeration class and the hasMoreElements and nextElement methods
		jclass enumerationClass = Env->FindClass("java/util/Enumeration");
		jmethodID hasMoreElementsMethod = Env->GetMethodID(enumerationClass, "hasMoreElements", "()Z");
		jmethodID nextElementMethod = Env->GetMethodID(enumerationClass, "nextElement", "()Ljava/lang/Object;");

		// Loop through the network interfaces and their addresses to find the private IP address
		while (Env->CallBooleanMethod(networkInterfacesObject, hasMoreElementsMethod)) {
			jobject networkInterfaceObject = Env->CallObjectMethod(networkInterfacesObject, nextElementMethod);

			// Find the getInetAddresses method of the NetworkInterface class
			jmethodID getInetAddressesMethod = Env->GetMethodID(networkInterfaceClass, "getInetAddresses", "()Ljava/util/Enumeration;");
			jobject addressesObject = Env->CallObjectMethod(networkInterfaceObject, getInetAddressesMethod);

			while (Env->CallBooleanMethod(addressesObject, hasMoreElementsMethod)) {
				jobject addressObject = Env->CallObjectMethod(addressesObject, nextElementMethod);
				jboolean isLoopback = Env->CallBooleanMethod(addressObject, isLoopbackAddressMethod);
				jboolean isSiteLocal = Env->CallBooleanMethod(addressObject, isSiteLocalAddressMethod);
				if (!isLoopback && isSiteLocal) {
					jmethodID getHostAddressMethod = Env->GetMethodID(inetAddressClass, "getHostAddress", "()Ljava/lang/String;");
					jstring ipAddressString = static_cast<jstring>(Env->CallObjectMethod(addressObject, getHostAddressMethod));
					const char* ipAddressChars = Env->GetStringUTFChars(ipAddressString, nullptr);
					ipAddress = FString(ipAddressChars);
					ipAddress += ":3000";
					Env->ReleaseStringUTFChars(ipAddressString, ipAddressChars);
					break;
				}
			}
			if (ipAddress != "unknown") {
				break;
			}
		}
		return ipAddress;
	}
	else
	{
	UE_LOG(LogAndroid, Warning, TEXT("ERROR: Could not get Java ENV\n"));
	return "unkown";
	}
#else
	return ipAddress;
#endif
}