#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <ua_pubsub_internal.h>

#define REPETITIONS 10

int timeval_subtract(struct timespec *result, struct timespec *x, struct timespec *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_nsec < y->tv_nsec)
    {
        int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
        y->tv_nsec -= 1000000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_nsec - y->tv_nsec > 1000000000)
    {
        int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
        y->tv_nsec += 1000000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_nsec = x->tv_nsec - y->tv_nsec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

// -Method to get second resolution timestamp
double time_measure_get_seconds()
{
    struct timespec ts2;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts2);
    return ts2.tv_sec;
}

#define TIME_MEASURE_START(X) clock_gettime(CLOCK_MONOTONIC_RAW, &X)
#define TIME_MEASURE_DIFF_USEC(X, TARGET)                                   \
    {                                                                       \
        struct timespec _diff_time_end, _diff_time;                         \
        clock_gettime(CLOCK_MONOTONIC_RAW, &_diff_time_end);                \
        timeval_subtract(&_diff_time, &_diff_time_end, &X);                 \
        (TARGET) = _diff_time.tv_sec * 1000000 + _diff_time.tv_nsec / 1000; \
    }

typedef enum
{
    PUB,
    SUB
} EntityType;

bool dataReceived = false;
int max_message_pow = 10;

UA_ByteString byteStringPayloadData = {0, NULL};
UA_ByteString fullPayloadData = {0, NULL};

UA_NodeId pubConnectionIdent, publishedDataSetIdent, writerGroupIdent,
    dataSetWriterIdent;

UA_NodeId subConnectionIdent;
UA_NodeId readerGroupIdentifier;
UA_NodeId readerIdentifier;
UA_DataSetReaderConfig readerConfig;

static void fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData);

void addByteStringDataSetField(UA_Server *server);

static void onVariableValueChanged(UA_Server *server,
                                   const UA_NodeId *sessionId,
                                   void *sessionContext,
                                   const UA_NodeId *nodeId,
                                   void *nodeContext,
                                   const UA_NumericRange *range,
                                   const UA_DataValue *data);

static void
addReaderGroup(UA_Server *server)
{
    UA_ReaderGroupConfig readerGroupConfig;
    memset(&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup1");
    UA_Server_addReaderGroup(server, subConnectionIdent, &readerGroupConfig,
                             &readerGroupIdentifier);
}

static void
addDataSetReader(UA_Server *server)
{
    memset(&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = UA_STRING("DataSet Reader 1");
    UA_UInt16 publisherIdentifier = 2240;
    readerConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    readerConfig.publisherId.id.uint16 = publisherIdentifier;
    readerConfig.writerGroupId = 101;
    readerConfig.dataSetWriterId = 62542;

    /* Setting up Meta data configuration in DataSetReader */
    fillTestDataSetMetaData(&readerConfig.dataSetMetaData);

    UA_Server_addDataSetReader(server, readerGroupIdentifier, &readerConfig,
                               &readerIdentifier);
}

static void
onVariableValueChanged(UA_Server *server,
                       const UA_NodeId *sessionId,
                       void *sessionContext,
                       const UA_NodeId *nodeId,
                       void *nodeContext,
                       const UA_NumericRange *range,
                       const UA_DataValue *data)
{
    (void)sessionId;
    (void)sessionContext;
    (void)nodeContext;
    (void)range;

    UA_Variant value;
    UA_Variant_init(&value);

    // Ler o valor atualizado da variável
    UA_StatusCode retval = UA_Server_readValue(server, *nodeId, &value);
    if (retval == UA_STATUSCODE_GOOD)
    {
        // Verificar o tipo de dado e exibir o valor
        if (value.type == &UA_TYPES[UA_TYPES_BYTESTRING])
        {
            UA_ByteString *byteStringValue = (UA_ByteString *)value.data;
            dataReceived = true;
            printf("Variable [%u] updated: UA_ByteString length=%zu, data=%s\n",
                   nodeId->identifier.numeric, byteStringValue->length, byteStringValue->data);
        }
        else
        {
            printf("Variable [%u] updated: Unsupported data type\n", nodeId->identifier.numeric);
        }
    }
    else
    {
        printf("Failed to read updated value for NodeId [%u]\n", nodeId->identifier.numeric);
    }

    UA_Variant_clear(&value);
}

static void
addSubscribedVariables(UA_Server *server, UA_NodeId dataSetReaderId)
{
    UA_NodeId folderId;
    UA_String folderName = readerConfig.dataSetMetaData.name;
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    UA_QualifiedName folderBrowseName;
    if (folderName.length > 0)
    {
        oAttr.displayName.locale = UA_STRING("en-US");
        oAttr.displayName.text = folderName;
        folderBrowseName.namespaceIndex = 1;
        folderBrowseName.name = folderName;
    }
    else
    {
        oAttr.displayName = UA_LOCALIZEDTEXT("en-US", "Subscribed Variables");
        folderBrowseName = UA_QUALIFIEDNAME(1, "Subscribed Variables");
    }

    UA_Server_addObjectNode(server, UA_NODEID_NULL,
                            UA_NS0ID(OBJECTSFOLDER), UA_NS0ID(ORGANIZES),
                            folderBrowseName, UA_NS0ID(BASEOBJECTTYPE), oAttr, NULL, &folderId);

    UA_FieldTargetVariable *targetVars = (UA_FieldTargetVariable *)
        UA_calloc(readerConfig.dataSetMetaData.fieldsSize, sizeof(UA_FieldTargetVariable));
    for (size_t i = 0; i < readerConfig.dataSetMetaData.fieldsSize; i++)
    {
        /* Variable to subscribe data */
        UA_VariableAttributes vAttr = UA_VariableAttributes_default;
        UA_LocalizedText_copy(&readerConfig.dataSetMetaData.fields[i].description,
                              &vAttr.description);
        vAttr.displayName.locale = UA_STRING("en-US");
        vAttr.displayName.text = readerConfig.dataSetMetaData.fields[i].name;
        vAttr.dataType = readerConfig.dataSetMetaData.fields[i].dataType;

        UA_NodeId newNode;
        UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1, (UA_UInt32)i + 50000),
                                  folderId, UA_NS0ID(HASCOMPONENT),
                                  UA_QUALIFIEDNAME(1, (char *)readerConfig.dataSetMetaData.fields[i].name.data),
                                  UA_NS0ID(BASEDATAVARIABLETYPE),
                                  vAttr, NULL, &newNode);

        /* For creating Targetvariables */
        UA_FieldTargetDataType_init(&targetVars[i].targetVariable);
        targetVars[i].targetVariable.attributeId = UA_ATTRIBUTEID_VALUE;
        targetVars[i].targetVariable.targetNodeId = newNode;

        UA_ValueCallback callback;
        callback.onRead = NULL;
        callback.onWrite = onVariableValueChanged;
        UA_Server_setVariableNode_valueCallback(server, newNode, callback);
    }

    UA_Server_DataSetReader_createTargetVariables(server, dataSetReaderId,
                                                  readerConfig.dataSetMetaData.fieldsSize,
                                                  targetVars);
    for (size_t i = 0; i < readerConfig.dataSetMetaData.fieldsSize; i++)
        UA_FieldTargetDataType_clear(&targetVars[i].targetVariable);

    UA_free(targetVars);
    UA_free(readerConfig.dataSetMetaData.fields);
}

static void
fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData)
{
    UA_DataSetMetaDataType_init(pMetaData);
    pMetaData->name = UA_STRING("DataSet 1");

    pMetaData->fieldsSize = 1;
    pMetaData->fields = (UA_FieldMetaData *)UA_Array_new(pMetaData->fieldsSize,
                                                         &UA_TYPES[UA_TYPES_FIELDMETADATA]);

    UA_FieldMetaData_init(&pMetaData->fields[0]);
    UA_NodeId_copy(&UA_TYPES[UA_TYPES_BYTESTRING].typeId, &pMetaData->fields[0].dataType);
    pMetaData->fields[0].builtInType = UA_NS0ID_BYTESTRING;
    pMetaData->fields[0].name = UA_STRING("ByteString");
    pMetaData->fields[0].valueRank = -1;
}

static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl, UA_NodeId *connectionIdent, EntityType type)
{
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("UADP Connection 1");
    connectionConfig.transportProfileUri = *transportProfile;
    UA_Variant_setScalar(&connectionConfig.address, networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    connectionConfig.publisherId.id.uint16 = type == PUB ? 2234 : UA_UInt32_random();
    UA_Server_addPubSubConnection(server, &connectionConfig, connectionIdent);
}

static void
addPublishedDataSet(UA_Server *server)
{
    UA_PublishedDataSetConfig publishedDataSetConfig;
    memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    publishedDataSetConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    publishedDataSetConfig.name = UA_STRING("Demo PDS");
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig, &publishedDataSetIdent);
}

void addByteStringVariable(UA_Server *server)
{

    UA_NodeId byteStringNodeId = UA_NODEID_STRING(1, "ByteStringVariable");
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "ByteStringVariable");
    attr.dataType = UA_TYPES[UA_TYPES_BYTESTRING].typeId;
    attr.valueRank = -1;

    UA_Variant_setScalar(&attr.value, &byteStringPayloadData, &UA_TYPES[UA_TYPES_BYTESTRING]);

    UA_Server_addVariableNode(server, byteStringNodeId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                              UA_QUALIFIEDNAME(1, "ByteStringVariable"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);
}

void addByteStringDataSetField(UA_Server *server)
{
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("ByteStringVariable");
    dataSetFieldConfig.field.variable.promotedField = false;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
        UA_NODEID_STRING(1, "ByteStringVariable");
    dataSetFieldConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, publishedDataSetIdent,
                              &dataSetFieldConfig, &dataSetFieldIdent);
}

static void
addWriterGroup(UA_Server *server)
{

    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = 10000000;
    writerGroupConfig.writerGroupId = 100;
    // writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;

    UA_UadpWriterGroupMessageDataType writerGroupMessage;
    UA_UadpWriterGroupMessageDataType_init(&writerGroupMessage);
    writerGroupMessage.networkMessageContentMask =
        (UA_UadpNetworkMessageContentMask)(UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);

    UA_ExtensionObject_setValue(&writerGroupConfig.messageSettings, &writerGroupMessage,
                                &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE]);

    UA_Server_addWriterGroup(server, pubConnectionIdent, &writerGroupConfig, &writerGroupIdent);
}

static void
addDataSetWriter(UA_Server *server)
{
    /* We need now a DataSetWriter within the WriterGroup. This means we must
     * create a new DataSetWriterConfig and add call the addWriterGroup function. */
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = 62541;
    dataSetWriterConfig.keyFrameCount = 10;
    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent,
                               &dataSetWriterConfig, &dataSetWriterIdent);
}

void executePubSubComunication(UA_Server *server, long long int *duration)
{
    struct timespec time_start;
    dataReceived = false;

    TIME_MEASURE_START(time_start);

    UA_Server_triggerWriterGroupPublish(server, writerGroupIdent);
    printf("Mensagem publicada!\n");

    while (!dataReceived)
    {
        UA_Server_run_iterate(server, true);
    }

    TIME_MEASURE_DIFF_USEC(time_start, *duration);
    // printf("o while executou %d vezes\n", contador);
    printf("Mensagem recebida - %lld\n", *duration);
}

void runTests(UA_Server *server)
{

    long long int duration[max_message_pow][REPETITIONS];

    for (size_t power = 1; power < (size_t)max_message_pow; power++)
    {
        int messageLength = (int)pow(2, power);
        byteStringPayloadData.length = (size_t)messageLength;

        // Writing message with new size on the nodeId
        UA_Variant value;
        UA_Variant_init(&value);
        UA_Variant_setScalar(&value, &byteStringPayloadData, &UA_TYPES[UA_TYPES_BYTESTRING]);
        UA_Server_writeValue(server, UA_NODEID_STRING(1, "ByteStringVariable"), value);

        for (int i = 0; i < REPETITIONS; i++)
        {
            executePubSubComunication(server, &(duration[power - 1][i]));
        }
    }

    // Write data on file

    const char *filename = "dataColected.csv";
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        perror("Erro ao abrir o arquivo");
        return;
    }

    fprintf(file, "Iteration, PayloadSize [bytes], Duration [usec]");

    for (int power = 1; power < max_message_pow; power++)
    {
        int size = (int)pow(2, power);
        for (int i = 0; i < REPETITIONS; i++)
        {
            fprintf(file, "%d, %d, %lld\n", i, size, duration[power - 1][i]);
        }
    }

    fclose(file);

    printf("Arquivo '%s' criado com sucesso!\n", filename);
}

static int
run(UA_String *transportProfile, UA_NetworkAddressUrlDataType *pubNetworkAddressUrl, UA_NetworkAddressUrlDataType *subNetworkAddressUrl)
{
    UA_Server *server = UA_Server_new();

    // publish
    addPubSubConnection(server, transportProfile, pubNetworkAddressUrl, &pubConnectionIdent, PUB);
    addPublishedDataSet(server);
    addByteStringVariable(server);
    addByteStringDataSetField(server);
    addWriterGroup(server);
    addDataSetWriter(server);
    // subscribe
    addPubSubConnection(server, transportProfile, subNetworkAddressUrl, &subConnectionIdent, SUB);
    addReaderGroup(server);
    addDataSetReader(server);
    addSubscribedVariables(server, readerIdentifier);

    UA_Server_enableAllPubSubComponents(server);

    UA_Server_run_startup(server);
    UA_Server_run_iterate(server, true);

    runTests(server);

    UA_Server_run_shutdown(server);
    UA_Server_delete(server);
    return 0;
}

int main(int argc, char **argv)
{
    UA_Byte customSizeByte[(int)pow(2, max_message_pow)];

    for (int i = 0; i < (int)sizeof(customSizeByte); i++)
    {
        customSizeByte[i] = '*';
    }
    fullPayloadData.length = (int)pow(2, max_message_pow);
    fullPayloadData.data = &customSizeByte[0];
    byteStringPayloadData.data = fullPayloadData.data;

    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType pubNetworkAddressUrl =
        {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_NetworkAddressUrlDataType subNetworkAddressUrl =
        {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4841/")};

    return run(&transportProfile, &pubNetworkAddressUrl, &subNetworkAddressUrl);
}