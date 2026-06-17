#include <android/log.h>
#include <jni.h>
#include <string.h>
#include <unistd.h>

namespace tflite {

#define LOG_TAG "TfLiteNpuFeature"

#define NULL_CHECK(ptr)                                                     \
  if (ptr == nullptr) {                                                     \
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s is null at line %d", \
                        #ptr, __LINE__);                                    \
    return false;                                                           \
  }

static JavaVM* g_jvm = nullptr;

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  g_jvm = vm;
  return JNI_VERSION_1_6;
}

bool DoesProcessHaveNpuFeatureAccess() {
  if (getuid() == 2000 || getuid() == 0) {
    // Allow NPU access from the Shell or Root.
    return true;
  }
  if (g_jvm == nullptr) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "g_jvm is null");
    return false;
  }

  JNIEnv* env = nullptr;

  if (JNI_OK != g_jvm->AttachCurrentThread(&env, nullptr) || env == nullptr) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "failed to get jni env");
    return false;
  }

  jclass activityThread = env->FindClass("android/app/ActivityThread");
  NULL_CHECK(activityThread);
  jmethodID currentActivityThread =
      env->GetStaticMethodID(activityThread, "currentActivityThread",
                             "()Landroid/app/ActivityThread;");
  NULL_CHECK(currentActivityThread);
  jobject activityThreadObj =
      env->CallStaticObjectMethod(activityThread, currentActivityThread);
  NULL_CHECK(activityThreadObj);

  jmethodID getApplication = env->GetMethodID(activityThread, "getApplication",
                                              "()Landroid/app/Application;");
  NULL_CHECK(getApplication);
  jobject context = env->CallObjectMethod(activityThreadObj, getApplication);
  NULL_CHECK(context);
  jclass contextClass = env->GetObjectClass(context);
  NULL_CHECK(contextClass);
  jmethodID getPackageManager =
      env->GetMethodID(contextClass, "getPackageManager",
                       "()Landroid/content/pm/PackageManager;");
  NULL_CHECK(getPackageManager);
  jobject packageManager = env->CallObjectMethod(context, getPackageManager);
  NULL_CHECK(packageManager);
  jclass packageManagerClass = env->GetObjectClass(packageManager);
  NULL_CHECK(packageManagerClass);
  jmethodID getPackageName = env->GetMethodID(
      packageManagerClass, "getPackagesForUid", "(I)[Ljava/lang/String;");
  NULL_CHECK(getPackageName);
  jobjectArray packageNames = (jobjectArray)env->CallObjectMethod(
      packageManager, getPackageName, getuid());
  NULL_CHECK(packageNames);
  jsize numPackages = env->GetArrayLength(packageNames);
  for (int i = 0; i < numPackages; i++) {
    jstring packageName = (jstring)env->GetObjectArrayElement(packageNames, i);
    NULL_CHECK(packageName);
    jmethodID getPackageInfo = env->GetMethodID(
        packageManagerClass, "getPackageInfo",
        "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
    NULL_CHECK(getPackageInfo);
    jobject packageInfo =
        env->CallObjectMethod(packageManager, getPackageInfo, packageName,
                              0x00004000 /* GET_CONFIGURATIONS*/);
    NULL_CHECK(packageInfo);
    jclass packageInfoClass = env->GetObjectClass(packageInfo);
    NULL_CHECK(packageInfoClass);

    jfieldID applicationInfoField =
        env->GetFieldID(packageInfoClass, "applicationInfo",
                        "Landroid/content/pm/ApplicationInfo;");
    NULL_CHECK(applicationInfoField);
    jobject applicationInfo =
        env->GetObjectField(packageInfo, applicationInfoField);
    NULL_CHECK(applicationInfo);
    jclass applicationInfoClass = env->GetObjectClass(applicationInfo);
    NULL_CHECK(applicationInfoClass);
    jfieldID targetSdkVersionField =
        env->GetFieldID(applicationInfoClass, "targetSdkVersion", "I");
    NULL_CHECK(targetSdkVersionField);
    jint targetSdkVersion =
        env->GetIntField(applicationInfo, targetSdkVersionField);

    if (targetSdkVersion < 37) {
      return true;
    }

    jfieldID reqFeaturesField = env->GetFieldID(
        packageInfoClass, "reqFeatures", "[Landroid/content/pm/FeatureInfo;");
    NULL_CHECK(reqFeaturesField);
    jobject reqFeaturesObj = env->GetObjectField(packageInfo, reqFeaturesField);
    if (reqFeaturesObj == nullptr) {
      return false;
    }
    jobjectArray features = (jobjectArray)reqFeaturesObj;

    NULL_CHECK(features);
    jsize numFeatures = env->GetArrayLength(features);
    for (int i = 0; i < numFeatures; i++) {
      jobject feature = env->GetObjectArrayElement(features, i);
      jclass featureClass = env->GetObjectClass(feature);
      NULL_CHECK(featureClass);
      jfieldID name =
          env->GetFieldID(featureClass, "name", "Ljava/lang/String;");
      NULL_CHECK(name);
      jfieldID nameField =
          env->GetFieldID(featureClass, "name", "Ljava/lang/String;");
      NULL_CHECK(nameField);
      jstring featureName = (jstring)env->GetObjectField(feature, nameField);
      NULL_CHECK(featureName);
      const char* featureNameStr =
          env->GetStringUTFChars((jstring)featureName, nullptr);
      NULL_CHECK(featureNameStr);

      if (!strcmp(featureNameStr, "android.hardware.npu")) {
        env->ReleaseStringUTFChars(featureName, featureNameStr);

        return true;
      }
      env->ReleaseStringUTFChars(featureName, featureNameStr);
    }
  }
  return false;
}

}  // namespace tflite
