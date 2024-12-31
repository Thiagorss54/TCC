/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

/**
 * .. _pubsub-tutorial:
 *
 * Working with Publish/Subscribe
 * ------------------------------
 *
 * Work in progress: This Tutorial will be continuously extended during the next
 * PubSub batches. More details about the PubSub extension and corresponding
 * open62541 API are located here: :ref:`pubsub`.
 *
 * Publishing Fields
 * ^^^^^^^^^^^^^^^^^
 * The PubSub publish example demonstrates the simplest way to publish
 * information from the information model over UDP multicast using the UADP
 * encoding.
 *
 * **Connection handling**
 *
 * PubSubConnections can be created and deleted on runtime. More details about
 * the system preconfiguration and connection can be found in
 * ``tutorial_pubsub_connection.c``.
 */

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/types.h>

#include <stdio.h>
#include <stdlib.h>

typedef enum
{
    PUB,
    SUB
} EntityType;

bool dataReceived = false;
int max_message_pow = 8;
UA_ByteString byteStringPayloadData = {0, NULL};

void updateLargeCustomVariable(UA_Server *server);

UA_NodeId pubConnectionIdent, subConnectionIdent, publishedDataSetIdent, writerGroupIdent,
    dataSetWriterIdent;

UA_NodeId readerGroupIdent, readerIdent;

static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl, UA_NodeId *connectionIdent, EntityType type)
{
    /* Details about the connection configuration and handling are located
     * in the pubsub connection tutorial */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("UADP Connection 1");
    connectionConfig.transportProfileUri = *transportProfile;
    UA_Variant_setScalar(&connectionConfig.address, networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    /* Changed to static publisherId from random generation to identify
     * the publisher on Subscriber side */
    connectionConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    connectionConfig.publisherId.id.uint16 = type == PUB ? 2234 : UA_UInt32_random();
    UA_Server_addPubSubConnection(server, &connectionConfig, connectionIdent);
}

/**
 * **PublishedDataSet handling**
 *
 * The PublishedDataSet (PDS) and PubSubConnection are the toplevel entities and
 * can exist alone. The PDS contains the collection of the published fields. All
 * other PubSub elements are directly or indirectly linked with the PDS or
 * connection. */
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

/**
 * **DataSetField handling**
 *
 * The DataSetField (DSF) is part of the PDS and describes exactly one published
 * field. */

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

/**
 * **WriterGroup handling**
 *
 * The WriterGroup (WG) is part of the connection and contains the primary
 * configuration parameters for the message creation. */
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

/**
 * **DataSetWriter handling**
 *
 * A DataSetWriter (DSW) is the glue between the WG and the PDS. The DSW is
 * linked to exactly one PDS and contains additional information for the
 * message generation. */
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

//--------------------------SubFunctions------------------------

UA_DataSetReaderConfig readerConfig;

static void fillTestDataSetMetaData(UA_DataSetMetaDataType *pMetaData);

static void
addReaderGroup(UA_Server *server)
{
    UA_ReaderGroupConfig readerGroupConfig;
    memset(&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup1");
    UA_Server_addReaderGroup(server, subConnectionIdent, &readerGroupConfig,
                             &readerGroupIdent);
}

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
    UA_UInt16 publisherIdentifier = 2235;
    readerConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    readerConfig.publisherId.id.uint16 = publisherIdentifier;
    readerConfig.writerGroupId = 100;
    readerConfig.dataSetWriterId = 62541;

    /* Setting up Meta data configuration in DataSetReader */
    fillTestDataSetMetaData(&readerConfig.dataSetMetaData);

    UA_Server_addDataSetReader(server, readerGroupIdent, &readerConfig,
                               &readerIdent);
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

static int
run(UA_String *transportProfile, UA_NetworkAddressUrlDataType *pubNetworkAddressUrl, UA_NetworkAddressUrlDataType *subNetworkAddressUrl)
{
    /* Create a server */
    UA_Server *server = UA_Server_new();

    // publish
    // addPubSubConnection(server, transportProfile, pubNetworkAddressUrl, &pubConnectionIdent, PUB);
    // addPublishedDataSet(server);
    // addByteStringVariable(server);
    // addByteStringDataSetField(server);
    // addWriterGroup(server);
    // addDataSetWriter(server);

    // subscribe
    addPubSubConnection(server, transportProfile, subNetworkAddressUrl, &subConnectionIdent, SUB);
    addReaderGroup(server);
    addDataSetReader(server);
    addSubscribedVariables(server, readerIdent);

    /* Enable the PubSubComponents */
    UA_Server_enableAllPubSubComponents(server);

    //     UA_Server_triggerWriterGroupPublish(server, writerGroupIdent);

    UA_Server_runUntilInterrupt(server);

    UA_Server_run_shutdown(server);
    /* Delete the server */
    UA_Server_delete(server);
    // return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    UA_Byte customSizeByte[(int)pow(2, max_message_pow)];

    for (int i = 0; i < sizeof(customSizeByte); i++)
    {
        customSizeByte[i] = '*';
    }
    byteStringPayloadData.length = (int)pow(2, max_message_pow);
    byteStringPayloadData.data = &customSizeByte[0];

    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType pubNetworkAddressUrl =
        {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_NetworkAddressUrlDataType subNetworkAddressUrl =
        {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4841/")};

    return run(&transportProfile, &pubNetworkAddressUrl, &subNetworkAddressUrl);
}
