/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information.
 *
 * Copyright (c) 2019 Kalycito Infotech Private Limited
 * Copyright (c) 2024 Fraunhofer IOSB (Author: Julius Pfrommer)
 */

/**
 * .. _pubsub-subscribe-tutorial:
 *
 * Subscribing Fields
 * ^^^^^^^^^^^^^^^^^^
 * The PubSub subscribe example demonstrates the simplest way to receive
 * information over two transport layers such as UDP and Ethernet, that are
 * published by tutorial_pubsub_publish example and update values in the
 * TargetVariables of Subscriber Information Model. */

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

UA_NodeId subConnectionIdent,
    pubConnectionIdent;
UA_NodeId readerGroupIdentifier;
UA_NodeId readerIdentifier;

UA_NodeId publishedDataSetIdent, writerGroupIdent, dataSetWriterIdent;

int max_message_pow = 8;
UA_ByteString byteStringPayloadData = {0, NULL};

UA_DataSetReaderConfig readerConfig;
int count = 1;
// int publisherId = 2234;

static void fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData);

void arrayToDateTime(UA_DateTime *receivedTime, const UA_Byte array[68]);

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

/**
 * **ReaderGroup**
 *
 * ReaderGroup is used to group a list of DataSetReaders. All ReaderGroups are
 * created within a PubSubConnection and automatically deleted if the connection
 * is removed. All network message related filters are only available in the DataSetReader. */
static void
addReaderGroup(UA_Server *server)
{
    UA_ReaderGroupConfig readerGroupConfig;
    memset(&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup1");
    UA_Server_addReaderGroup(server, subConnectionIdent, &readerGroupConfig,
                             &readerGroupIdentifier);
}

/**
 * **DataSetReader**
 *
 * DataSetReader can receive NetworkMessages with the DataSetMessage
 * of interest sent by the Publisher. DataSetReader provides
 * the configuration necessary to receive and process DataSetMessages
 * on the Subscriber side. DataSetReader must be linked with a
 * SubscribedDataSet and be contained within a ReaderGroup. */
static void
addDataSetReader(UA_Server *server)
{
    memset(&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = UA_STRING("DataSet Reader 1");
    /* Parameters to filter which DataSetMessage has to be processed
     * by the DataSetReader */
    /* The following parameters are used to show that the data published by
     * tutorial_pubsub_publish.c is being subscribed and is being updated in
     * the information model */
    UA_UInt16 publisherIdentifier = 2234;
    readerConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    readerConfig.publisherId.id.uint16 = publisherIdentifier;
    readerConfig.writerGroupId = 100;
    readerConfig.dataSetWriterId = 62541;

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
    UA_Server_triggerWriterGroupPublish(server, writerGroupIdent);
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

/**
 * **SubscribedDataSet**
 *
 * Set SubscribedDataSet type to TargetVariables data type.
 * Add subscribedvariables to the DataSetReader */
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

    /**
     * **TargetVariables**
     *
     * The SubscribedDataSet option TargetVariables defines a list of Variable mappings between
     * received DataSet fields and target Variables in the Subscriber AddressSpace.
     * The values subscribed from the Publisher are updated in the value field of these variables */
    /* Create the TargetVariables with respect to DataSetMetaData fields */
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

/**
 * **DataSetMetaData**
 *
 * The DataSetMetaData describes the content of a DataSet. It provides the information necessary to decode
 * DataSetMessages on the Subscriber side. DataSetMessages received from the Publisher are decoded into
 * DataSet and each field is updated in the Subscriber based on datatype match of TargetVariable fields of Subscriber
 * and PublishedDataSetFields of Publisher */
static void
fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData)
{
    UA_DataSetMetaDataType_init(pMetaData);
    pMetaData->name = UA_STRING("DataSet 1");

    /* Static definition of number of fields size to 4 to create four different
     * targetVariables of distinct datatype
     * Currently the publisher sends only DateTime data type */
    pMetaData->fieldsSize = 1;
    pMetaData->fields = (UA_FieldMetaData *)UA_Array_new(pMetaData->fieldsSize,
                                                         &UA_TYPES[UA_TYPES_FIELDMETADATA]);

    /* ByteString DataType*/
    UA_FieldMetaData_init(&pMetaData->fields[0]);
    UA_NodeId_copy(&UA_TYPES[UA_TYPES_BYTESTRING].typeId, &pMetaData->fields[0].dataType);
    pMetaData->fields[0].builtInType = UA_NS0ID_BYTESTRING;
    pMetaData->fields[0].name = UA_STRING("ByteString");
    pMetaData->fields[0].valueRank = -1;
}

//--------------------------PubFunctions------------------------

static void
addPublishedDataSet(UA_Server *server)
{
    /* The PublishedDataSetConfig contains all necessary public
     * information for the creation of a new PublishedDataSet */
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
    /* Now we create a new WriterGroupConfig and add the group to the existing
     * PubSubConnection. */
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = 1000;
    writerGroupConfig.writerGroupId = 100;
    // writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;

    /* Change message settings of writerGroup to send PublisherId,
     * WriterGroupId in GroupHeader and DataSetWriterId in PayloadHeader
     * of NetworkMessage */
    UA_UadpWriterGroupMessageDataType writerGroupMessage;
    UA_UadpWriterGroupMessageDataType_init(&writerGroupMessage);
    writerGroupMessage.networkMessageContentMask =
        (UA_UadpNetworkMessageContentMask)(UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |
                                           UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);

    /* The configuration flags for the messages are encapsulated inside the
     * message- and transport settings extension objects. These extension
     * objects are defined by the standard. e.g.
     * UadpWriterGroupMessageDataType */
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

static int
run(UA_String *transportProfile, UA_NetworkAddressUrlDataType *subNetworkAddressUrl, UA_NetworkAddressUrlDataType *pubNetworkAddressUrl)
{
    UA_Server *server = UA_Server_new();

    // subscribe
    // addPubSubConnection(server, transportProfile, subNetworkAddressUrl, &subConnectionIdent, SUB);
    // addReaderGroup(server);
    // addDataSetReader(server);
    // addSubscribedVariables(server, readerIdentifier);

    // publish
    addPubSubConnection(server, transportProfile, pubNetworkAddressUrl, &pubConnectionIdent, PUB);
    addPublishedDataSet(server);
    addByteStringVariable(server);
    addByteStringDataSetField(server);
    addWriterGroup(server);
    addDataSetWriter(server);

    UA_Server_enableAllPubSubComponents(server);
    UA_Server_runUntilInterrupt(server);

    UA_Server_delete(server);
    return 0;
}

int main(int argc, char **argv)
{

    UA_Byte customSizeByte[(int)pow(2, max_message_pow)];

    for (int i = 0; i < sizeof(customSizeByte); i++)
    {
        customSizeByte[i] = '-';
    }
    byteStringPayloadData.length = (int)pow(2, max_message_pow);
    byteStringPayloadData.data = &customSizeByte[0];

    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType subNetworkAddressUrl =
        {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_NetworkAddressUrlDataType pubNetworkAddressUrl =
        {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4841/")};

    return run(&transportProfile, &subNetworkAddressUrl, &pubNetworkAddressUrl);
}
