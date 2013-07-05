/**
 *  @file ARNETWORK_JNIManager.c
 *  @brief JNI between the ARNETWORK_Manager.h and ARNETWORK_Manager.java
 *  @date 01/18/2013
 *  @author maxime.maitre@parrot.com
 */

/*****************************************
 *
 *             include file :
 *
 *****************************************/

#include <jni.h>
#include <stdlib.h>

#include <libARSAL/ARSAL_Print.h>

#include <libARNetwork/ARNETWORK_Error.h>
#include <libARNetwork/ARNETWORK_Manager.h>

#include <libARNetworkAL/ARNETWORKAL_Frame.h>

/**
 *  @brief data sent to the callbak
 */
typedef struct
{
    jobject jManager; /**< manager sent the data */
    jobject jARData; /**< java native data*/
}ARNETWORK_JNIManager_CallbackData_t;

/*****************************************
 *
 *             private header:
 *
 *****************************************/

#define ARNETWORK_JNIMANGER_TAG "JNIManager" /** tag used by the print of the file */

/**
 *  @brief call back use when the data are sent or have a timeout
 *  @param[in] IOBufferID identifier of the IoBuffer is calling back
 *  @param[in] dataPtr pointer on the data
 *  @param[in] customData pointer on a custom data
 *  @param[in] status indicating the reason of the callback. eARNETWORK_MANAGER_CALLBACK_STATUS type
 *  @return eARNETWORK_MANAGER_CALLBACK_RETURN
 *  @see eARNETWORK_MANAGER_CALLBACK_STATUS
 */
eARNETWORK_MANAGER_CALLBACK_RETURN ARNETWORK_JNIManager_Callback_t(int IOBufferID,  uint8_t *dataPtr, void *customData, eARNETWORK_MANAGER_CALLBACK_STATUS status);

/**
 *  @brief free the global references used by the callback
 *  @param[in] env java environment
 *  @param[in] callbackDataPtrAddr address of the pointer on the callbackData storing the global references
 *  @warning this funtion free the callbackData and set callbackDataPtrAddr to NULL
 */
void ARNETWORK_JNIManager_FreeCallbackData(JNIEnv *env, ARNETWORK_JNIManager_CallbackData_t **callbackDataPtrAddr);

/*****************************************
 *
 *             implementation :
 *
 *****************************************/


static JavaVM* gARNETWORK_JNIManager_VM = NULL; /** reference to the java virtual machine */
static jmethodID nativeDataSetUsed_id = 0;

/**
 *  @brief save the reference to the java virtual machine
 *  @note this function is automatically call on the JNI startup
 *  @param[in] VM reference to the java virtual machine
 *  @param[in] reserved data reserved
 *  @return JNI version
 */
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *VM, void *reserved)
{
    ARSAL_PRINT(ARSAL_PRINT_DEBUG, ARNETWORK_JNIMANGER_TAG, "Library has been loaded");

    /** Saving the reference to the java virtual machine */
    gARNETWORK_JNIManager_VM = VM;

    /** Return the JNI version */
    return JNI_VERSION_1_6;
}

/**
 *  @brief Create a new manager
 *  @warning This function allocate memory
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param[in] sendBufferSize size in byte of the sending buffer. ideally must be equal to the sum of the sizes of one data of all input buffers
 *  @param[in] numberOfInput Number of input buffer
 *  @param[in] inputParamArray array of the parameters of creation of the inputs. The array must contain as many parameters as the number of input buffer.
 *  @param[in] numberOfOutput Number of output buffer
 *  @param[in] outputParamArray array of the parameters of creation of the outputs. The array must contain as many parameters as the number of output buffer.
 *  @return Pointer on the ARNETWORK_Manager_t.
 *  @note This creator adds for all output, one other IOBuffer for storing the acknowledgment to return.
 * These new buffers are added in the input and output buffer arrays.
 *  @warning The identifiers of the IoBuffer should not exceed the value 128.
 *  @see Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeDelete()
 *
 */
JNIEXPORT jlong JNICALL
Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeNew(JNIEnv *env, jobject obj, jlong jOSSpecificManagerPtr, jint numberOfInput, jobjectArray inputParamArray, jint numberOfOutput, jobjectArray outputParamArray, jint timeBetweenPingsMs)
{
    /** -- Create a new manager -- */

    /** local declarations */
    ARNETWORK_Manager_t *managerPtr = NULL;
    ARNETWORK_IOBufferParam_t* pArrParamInput = NULL;
    ARNETWORK_IOBufferParam_t* pArrParamOutput = NULL;
    int ii = 0;
    eARNETWORK_ERROR error = ARNETWORK_OK;

    jfieldID fieldID;
    jclass IOBufferParam_cls;
    jobject jIOBuffer ;

    /** get the class IOBufferParam */
    IOBufferParam_cls = (*env)->FindClass(env, "com/parrot/arsdk/arnetwork/ARNetworkIOBufferParam");
    if (NULL == IOBufferParam_cls)
    {
        error = ARNETWORK_ERROR_ALLOC;
    }

    if(error == ARNETWORK_OK)
    {
        /** allocate the input parameters C */

        pArrParamInput = malloc ( sizeof(ARNETWORK_IOBufferParam_t) * numberOfInput );
        if(pArrParamInput == NULL && numberOfInput != 0 )
        {
            error = ARNETWORK_ERROR_ALLOC;
        }
    }

    if(error == ARNETWORK_OK)
    {
        /** copy the parameters java to the input parameters C*/

        for(ii = 0; ii < numberOfInput; ++ii)
        {
            /** get obj */
            jIOBuffer = (*env)->GetObjectArrayElement(env, inputParamArray, ii);
            fieldID = (*env)->GetFieldID(env, IOBufferParam_cls, "m_IOBufferParamPtr", "J" );

            pArrParamInput[ii] = *( (ARNETWORK_IOBufferParam_t*) (intptr_t) (*env)->GetLongField(env, jIOBuffer, fieldID) );
        }
    }

    if(error == ARNETWORK_OK)
    {
        /** allocate the output parameters C */

        pArrParamOutput = malloc ( sizeof(ARNETWORK_IOBufferParam_t) * numberOfOutput );
        if(pArrParamOutput == NULL && numberOfInput != 0 )
        {
            error = ARNETWORK_ERROR_ALLOC;
        }
    }

    if(error == ARNETWORK_OK)
    {
        /** copy the parameters java to the output parameters C*/

        for(ii = 0; ii<  numberOfOutput; ++ii)
        {
            /** get obj */
            jIOBuffer = (*env)->GetObjectArrayElement(env, outputParamArray, ii);
            fieldID = (*env)->GetFieldID(env, IOBufferParam_cls, "m_IOBufferParamPtr", "J");

            pArrParamOutput[ii] = *( (ARNETWORK_IOBufferParam_t*) (intptr_t) (*env)->GetLongField(env, jIOBuffer, fieldID) );
        }
    }

    if(error == ARNETWORK_OK)
    {
        /** create the manager */
        // TODO Add networkALManager object
        managerPtr = ARNETWORK_Manager_New((ARNETWORKAL_Manager_t*) (intptr_t) jOSSpecificManagerPtr, numberOfInput, pArrParamInput, numberOfOutput, pArrParamOutput, timeBetweenPingsMs, &error);
    }

    /** print error */
    if(error != ARNETWORK_OK)
    {
        ARSAL_PRINT(ARSAL_PRINT_ERROR, ARNETWORK_JNIMANGER_TAG, " error: %d occurred \n", error);
    }

    return (long) managerPtr;
}

/**
 *  @brief Delete the Manager
 *  @warning This function free memory
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param jManagerPtr adress of the ARNETWORK_Manager_t
 *  @see Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeNew()
 */
JNIEXPORT void JNICALL
Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeDelete(JNIEnv *env, jobject obj, jlong jManagerPtr)
{
    /** -- Delete the Manager -- */

    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;
    ARNETWORK_Manager_Delete(&managerPtr);
}

/**
 *  @brief Manage the sending of the data
 *  @warning This function must be called by a specific thread.
 *  @pre The OS specific Manager need to be defined and initialized with a network type nativeInitWiFiNetwork()
 *  @post Before join the thread calling this function, nativeStop() must be called.
 *  @note This function send the data stored in the input buffer through ARNETWORK_Manager_SendFixedSizeData().
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param jManagerPtr adress of the ARNETWORK_Manager_t
 *  @return NULL
 *  @see Java_com_parrot_arsdk_arnetwork_ARNetworkALManager_nativeInitWiFiNetwork()
 *  @see Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeStop()
 */
JNIEXPORT jint JNICALL
Java_com_parrot_arsdk_arnetwork_SendingRunnable_nativeSendingThreadRun(JNIEnv *env, jobject obj, jlong jManagerPtr)
{
    /** -- Manage the sending of the data -- */

    /** local declarations */
    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;

    return (int) ARNETWORK_Manager_SendingThreadRun( managerPtr );
}

/**
 *  @brief Manage the reception of the data.
 *  @warning This function must be called by a specific thread.
 *  @pre The OS specific Manager need to be defined and initialized with a network type nativeInitWiFiNetwork()
 *  @post Before join the thread calling this function, nativeStop() must be called.
 *  @note This function receives the data through ARNETWORK_Manager_ReadFixedSizeData() and stores them in the output buffers according to their parameters.
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param jManagerPtr adress of the ARNETWORK_Manager_t
 *  @return NULL
 *  @see Java_com_parrot_arsdk_arnetwork_ARNetworkALManager_nativeInitWiFiNetwork()
 *  @see Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeStop()
 */
JNIEXPORT jint JNICALL
Java_com_parrot_arsdk_arnetwork_ReceivingRunnable_nativeReceivingThreadRun(JNIEnv *env, jobject obj, jlong jManagerPtr)
{
    /** -- Manage the reception of the data -- */

    /** local declarations */
    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;

    return (int) ARNETWORK_Manager_ReceivingThreadRun(managerPtr);
}

/**
 *  @brief stop the threads of sending and reception
 *  @details Used to kill the threads calling nativeSendingThreadRun() and nativeReceivingThreadRun().
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param jManagerPtr adress of the ARNETWORK_Manager_t
 *  @see Java_com_parrot_arsdk_arnetwork_SendingRunnable_nativeSendingThreadRun()
 *  @see Java_com_parrot_arsdk_arnetwork_ReceivingRunnable_nativeReceivingThreadRun()
 */
JNIEXPORT void JNICALL
Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeStop(JNIEnv *env, jobject obj, jlong jManagerPtr)
{
    /** -- stop the threads of sending and reception -- */

    /** local declarations */
    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;

    ARNETWORK_Manager_Stop(managerPtr);
}

/**
 *  @brief Flush all buffers of the network manager
 *  @param managerPtr pointer on the Manager
 *  @return error eARNETWORK_ERROR
 */
JNIEXPORT jint JNICALL
Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeFlush(JNIEnv *env, jobject obj, jlong jManagerPtr)
{
    /** -- Flush all buffers of the network manager -- */

    /** local declarations */
    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;

    return ARNETWORK_Manager_Flush(managerPtr);
}

/**
 *  @brief Add data to send in a IOBuffer
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param jManagerPtr adress of the ARNETWORK_Manager_t
 *  @param[in] inputBufferID identifier of the input buffer in which the data must be stored
 *  @param[in] ARData ARNativeData to send
 *  @param[in] jdataPtr array of byte to send ( use arData.getData() )
 *  @param[in] dataSize size of the data to send ( use arData.getDataSize() )
 *  @param[in] doDataCopy indocator to copy the data in the ARNETWORK_Manager
 *  @return error eARNETWORK_ERROR
 */
JNIEXPORT jint JNICALL
Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeSendData(JNIEnv *env, jobject obj, jlong jManagerPtr, jint inputBufferID, jobject ARData, jlong jdataPtr, jint dataSize, jint doDataCopy)
{
    /** -- Add data to send in a IOBuffer using variable size data -- */

    /** local declarations */
    jobject dataObj = NULL;
    uint8_t *dataPtr = (uint8_t*) (intptr_t) jdataPtr;
    ARNETWORK_JNIManager_CallbackData_t *callbackDataPtr = NULL;
    eARNETWORK_ERROR error = ARNETWORK_OK;
    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;

    /** if NO copy data create a custom data to send to the callback*/
    if(!doDataCopy)
    {
        /** allocate the data send to the callback */
        callbackDataPtr = malloc( sizeof(ARNETWORK_JNIManager_CallbackData_t) );
        if(callbackDataPtr == NULL)
        {
            error = ARNETWORK_ERROR_ALLOC;
        }

        /** create a global reference of the java manager object delete by the callback */
        callbackDataPtr->jManager = (*env)->NewGlobalRef(env, obj);

        /** create a global reference of the java ARnativeData object delete by the callback */
        callbackDataPtr->jARData = (*env)->NewGlobalRef(env, ARData);
    }

    if(error == ARNETWORK_OK)
    {
        /** send the data */
        error = ARNETWORK_Manager_SendData( managerPtr, inputBufferID, dataPtr, dataSize, callbackDataPtr, &(ARNETWORK_JNIManager_Callback_t), doDataCopy);
    }

    return error;
}

/**
 *  @brief Read data received in a IOBuffer
 *  @warning blocking function
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param jManagerPtr adress of the ARNETWORK_Manager_t
 *  @param outputBufferID identifier of the output buffer in which the data must be read
 *  @param data pointer to the ARNativeData allocated memory
 *  @param capacity capacity of the ARNativeData allocated memory
 *  @param nativeData ARNativeData object (to set the used size)
 *  @return error eARNETWORK_ERROR type
 */
JNIEXPORT jint JNICALL
Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeReadData(JNIEnv *env, jobject obj, jlong jManagerPtr, jint outputBufferID, jlong data, jint capacity, jobject nativeData)
{
    /** -- Read data received in a IOBuffer using variable size data -- */

    /** local declarations */
    int readSize = 0;
    eARNETWORK_ERROR error = ARNETWORK_OK;
    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;
    uint8_t *dataPtr = (uint8_t *) (intptr_t) data;
    jboolean setRes = JNI_FALSE;

    if (nativeDataSetUsed_id == 0)
    {
        jclass clz = (*env)->GetObjectClass (env, nativeData);
        if (clz != 0)
        {
            nativeDataSetUsed_id = (*env)->GetMethodID (env, clz, "setUsedSize", "(I)Z");
            (*env)->DeleteLocalRef (env, clz);
        }
        else
        {
            error = ARNETWORK_ERROR;
            return error;
        }
    }

    error = ARNETWORK_Manager_ReadData( managerPtr, outputBufferID, dataPtr, capacity, &readSize );

    setRes = (*env)->CallBooleanMethod (env, nativeData, nativeDataSetUsed_id, readSize);
    if (setRes != JNI_TRUE)
    {
        error = ARNETWORK_ERROR_BUFFER_SIZE;
    }

    return error;
}

/**
 *  @brief try to read data received in a IOBuffer (non-blocking function)
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param jManagerPtr adress of the ARNETWORK_Manager_t
 *  @param outputBufferID identifier of the output buffer in which the data must be read
 *  @param data pointer to the ARNativeData allocated memory
 *  @param capacity capacity of the ARNativeData allocated memory
 *  @param nativeData ARNativeData object (to set the used size)
 *  @return error eARNETWORK_ERROR type
 */
JNIEXPORT jint JNICALL
Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeTryReadData(JNIEnv *env, jobject obj, jlong jManagerPtr, jint outputBufferID, jlong data, jint capacity, jobject nativeData)
{
    /** -- Read data received in a IOBuffer using variable size data -- */

    /** local declarations */
    int readSize = 0;
    eARNETWORK_ERROR error = ARNETWORK_OK;
    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;
    uint8_t *dataPtr = (uint8_t *) (intptr_t) data;
    jboolean setRes = JNI_FALSE;

    if (nativeDataSetUsed_id == 0)
    {
        jclass clz = (*env)->GetObjectClass (env, nativeData);
        if (clz != 0)
        {
            nativeDataSetUsed_id = (*env)->GetMethodID (env, clz, "setUsedSize", "(I)Z");
            (*env)->DeleteLocalRef (env, clz);
        }
        else
        {
            error = ARNETWORK_ERROR;
            return error;
        }
    }

    error = ARNETWORK_Manager_TryReadData( managerPtr, outputBufferID, dataPtr, capacity, &readSize );

    setRes = (*env)->CallBooleanMethod (env, nativeData, nativeDataSetUsed_id, readSize);
    if (setRes != JNI_TRUE)
    {
        error = ARNETWORK_ERROR_BUFFER_SIZE;
    }

    return error;
}

/**
 *  @brief Read, with timeout, a data received in IOBuffer
 *  @param env reference to the java environment
 *  @param obj reference to the object calling this function
 *  @param jManagerPtr adress of the ARNETWORK_Manager_t
 *  @param outputBufferID identifier of the output buffer in which the data must be read
 *  @param data pointer to the ARNativeData allocated memory
 *  @param capacity capacity of the ARNativeData allocated memory
 *  @param nativeData ARNativeData object (to set the used size)
 *  @param timeoutMs maximum time in millisecond to wait if there is no data to read
 *  @return error eARNETWORK_ERROR type
 */
JNIEXPORT jint JNICALL
Java_com_parrot_arsdk_arnetwork_ARNetworkManager_nativeReadDataWithTimeout(JNIEnv *env, jobject obj, jlong jManagerPtr, jint outputBufferID, jlong data, jint capacity, jobject nativeData, jint timeoutMs)
{
    /** -- Read, with timeout, a data received in IOBuffer using variable size data -- */

    /** local declarations */
    int readSize = 0;
    eARNETWORK_ERROR error = ARNETWORK_OK;
    ARNETWORK_Manager_t *managerPtr = (ARNETWORK_Manager_t*) (intptr_t) jManagerPtr;
    uint8_t *dataPtr = (uint8_t *) (intptr_t) data;
    jboolean setRes = JNI_FALSE;

    if (nativeDataSetUsed_id == 0)
    {
        jclass clz = (*env)->GetObjectClass (env, nativeData);
        if (clz != 0)
        {
            nativeDataSetUsed_id = (*env)->GetMethodID (env, clz, "setUsedSize", "(I)Z");
            (*env)->DeleteLocalRef (env, clz);
        }
        else
        {
            error = ARNETWORK_ERROR;
            return error;
        }
    }

    error = ARNETWORK_Manager_ReadDataWithTimeout( managerPtr, outputBufferID, dataPtr, capacity, &readSize, timeoutMs );

    setRes = (*env)->CallBooleanMethod (env, nativeData, nativeDataSetUsed_id, readSize);
    if (setRes != JNI_TRUE)
    {
        error = ARNETWORK_ERROR_BUFFER_SIZE;
    }

    return error;
}

eARNETWORK_MANAGER_CALLBACK_RETURN ARNETWORK_JNIManager_Callback_t(int IOBufferID, uint8_t *dataPtr, void *customData, eARNETWORK_MANAGER_CALLBACK_STATUS status)
{
    /** callback */

    /** local declarations */
    JNIEnv* env = NULL;
    eARNETWORK_ERROR error = ARNETWORK_OK;
    eARNETWORK_MANAGER_CALLBACK_RETURN callbackReturn = ARNETWORK_MANAGER_CALLBACK_RETURN_DEFAULT;
    int retry = 0;
    jclass managerCLS =  NULL;
    jmethodID jMethodCallback = 0;
    ARNETWORK_JNIManager_CallbackData_t *callbackDataPtr = (ARNETWORK_JNIManager_CallbackData_t*) customData;

    /** get the environment */
    if (gARNETWORK_JNIManager_VM != NULL)
    {
        (*gARNETWORK_JNIManager_VM)->GetEnv(gARNETWORK_JNIManager_VM, (void **)&env, JNI_VERSION_1_6);
    }

    /** check parameters:
     *  -   env is not null
     *  -   callbackDataPtr is not null
     *  -   jManager is not null
     */
    if(env != NULL &&
       callbackDataPtr != NULL &&
       callbackDataPtr->jManager != NULL)
    {
        /** send the callback java of the java manager */

        /** get the manager class reference */
        managerCLS = (*env)->GetObjectClass(env, callbackDataPtr->jManager);
        jMethodCallback = (*env)->GetMethodID(env, managerCLS, "callback", "(ILcom/parrot/arsdk/arsal/ARNativeData;I)I" );

        /** free the local reference on the class manager */
        (*env)->DeleteLocalRef(env, managerCLS);

        callbackReturn = (*env)->CallIntMethod(env, callbackDataPtr->jManager, jMethodCallback, IOBufferID, callbackDataPtr->jARData, status);

        switch(status)
        {
        case ARNETWORK_MANAGER_CALLBACK_STATUS_SENT :

            break;

        case ARNETWORK_MANAGER_CALLBACK_STATUS_ACK_RECEIVED :

            break;

        case ARNETWORK_MANAGER_CALLBACK_STATUS_TIMEOUT :

            break;

        case ARNETWORK_MANAGER_CALLBACK_STATUS_FREE :
            ARNETWORK_JNIManager_FreeCallbackData (env, &callbackDataPtr);
            break;

        default:

            break;
        }
    }
    else
    {
        error = ARNETWORK_ERROR_BAD_PARAMETER;
    }

    return callbackReturn;
}

void ARNETWORK_JNIManager_FreeCallbackData (JNIEnv* env, ARNETWORK_JNIManager_CallbackData_t** callbackDataPtrAddr)
{
    /** -- free the global references of the callback -- */

    /** local declarations */
    ARNETWORK_JNIManager_CallbackData_t *callbackDataPtr = NULL;

    if(callbackDataPtrAddr != NULL)
    {
        callbackDataPtr = *callbackDataPtrAddr;

        if(callbackDataPtr != NULL)
        {
            /** delete the java ARNativeData object reference */
            (*env)->DeleteGlobalRef( env, callbackDataPtr->jARData );

            /** delete the java manager object reference */
            (*env)->DeleteGlobalRef( env, callbackDataPtr->jManager );

            /** the callback data */
            free(callbackDataPtr);
            callbackDataPtr = NULL;
        }
        *callbackDataPtrAddr = NULL;
    }
}
