#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_pubsub.h>

#include <stdio.h>
#include <stdlib.h>

typedef enum
{
    PUB,
    SUB
} EntityType;

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

/* Add new connection to the server */
static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl, UA_NodeId *connectionIdent, EntityType type)
{
    /* Configuration creation for the connection */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.name = UA_STRING("UDPMC Connection 1");
    connectionConfig.transportProfileUri = *transportProfile;
    UA_Variant_setScalar(&connectionConfig.address, networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT32;
    connectionConfig.publisherId.id.uint32 = type == PUB ? 2235 : UA_UInt32_random();
    UA_Server_addPubSubConnection(server, &connectionConfig, connectionIdent);
}

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
    readerConfig.name = UA_STRING("DataSet Reader 2");
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

    // publish echo when message is received
    // UA_Server_triggerWriterGroupPublish(server, writerGroupIdent);
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

static int
run(UA_String *transportProfile, UA_NetworkAddressUrlDataType *subNetworkAddressUrl)
{
    UA_Server *server = UA_Server_new();

    // subscribe
    addPubSubConnection(server, transportProfile, subNetworkAddressUrl, &subConnectionIdent, SUB);
    addReaderGroup(server);
    addDataSetReader(server);
    addSubscribedVariables(server, readerIdentifier);

    UA_Server_enableAllPubSubComponents(server);
    UA_Server_runUntilInterrupt(server);

    UA_Server_delete(server);
    return 0;
}

int main(int argc, char **argv)
{
    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType subNetworkAddressUrl =
        {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4841/")};

    return run(&transportProfile, &subNetworkAddressUrl);
}
