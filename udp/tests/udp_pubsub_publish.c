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
int max_message_pow = 2;
UA_ByteString byteStringPayloadData = {0, NULL};

UA_NodeId pubConnectionIdent, publishedDataSetIdent, writerGroupIdent,
    dataSetWriterIdent;

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
    writerGroupConfig.publishingInterval = 1000;
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

static int
run(UA_String *transportProfile, UA_NetworkAddressUrlDataType *pubNetworkAddressUrl)
{
    UA_Server *server = UA_Server_new();

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

    return run(&transportProfile, &pubNetworkAddressUrl);
}
