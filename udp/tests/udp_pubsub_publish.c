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

int max_message_pow = 5;
UA_ByteString byteStringPayloadData = {0, NULL};

void updateLargeCustomVariable(UA_Server *server);

UA_NodeId pubConnectionIdent,subConnectionIdent, publishedDataSetIdent, writerGroupIdent,
    dataSetWriterIdent;

static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl, UA_NodeId *connectionIdent){
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
    connectionConfig.publisherId.id.uint16 =  2234;
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
addPublishedDataSet(UA_Server *server) {
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
static void
addDataSetField(UA_Server *server) {
    /* Add a field to the previous created PublishedDataSet */
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("Server localtime");
    dataSetFieldConfig.field.variable.promotedField = false;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
        UA_NS0ID(SERVER_SERVERSTATUS_CURRENTTIME);
    dataSetFieldConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, publishedDataSetIdent,
                              &dataSetFieldConfig, &dataSetFieldIdent);
}
//Teste da variável grande
static void
addLargeCustomVariable(UA_Server *server) {
    /* Define a large array of bytes (at least 1000 bytes) */
    UA_Byte largeData[68];
    for (size_t i = 0; i < sizeof(largeData); i++) {
        largeData[i] = (UA_Byte)(i % 256); // Fill the array with sample data
    }

    /* Get the current timestamp */
    UA_DateTime timestamp = UA_DateTime_now();

    // Add the timestamp in the last 8 bytes of the array
    *(UA_DateTime*)&largeData[60] = timestamp;

    /* Create a variable node in the server’s address space */
    UA_NodeId largeVariableNodeId = UA_NODEID_STRING(1, "LargeCustomVariable");
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "LargeCustomVariable");
    attr.dataType = UA_TYPES[UA_TYPES_BYTE].typeId;
    attr.valueRank = 1; // Indicates an array
    UA_UInt32 arrayDimensions[1] = {68};
    attr.arrayDimensions = arrayDimensions;
    attr.arrayDimensionsSize = 1;
    UA_Variant_setArray(&attr.value, largeData, 68, &UA_TYPES[UA_TYPES_BYTE]);

    UA_Server_addVariableNode(server, largeVariableNodeId,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                              UA_QUALIFIEDNAME(1, "LargeCustomVariable"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);
}

void updateLargeCustomVariable(UA_Server *server){
    UA_NodeId largeVariableNodeId = UA_NODEID_STRING(1, "LargeCustomVariable");

    UA_Byte largeData[68];
    for (size_t i = 0; i < sizeof(largeData); i++) {
        largeData[i] = (UA_Byte)(i % 256); // Fill the array with sample data
    }

    /* Get the current timestamp */
    UA_DateTime timestamp = UA_DateTime_now();

    // Add the timestamp in the last 8 bytes of the array
    *(UA_DateTime*)&largeData[60] = timestamp;

    UA_Variant value;
    UA_Variant_setArray(&value, largeData, 68, &UA_TYPES[UA_TYPES_BYTE]);
    
    UA_StatusCode status = UA_Server_writeValue(server, largeVariableNodeId, value);
    if (status == UA_STATUSCODE_GOOD) {
        printf("LargeCustomVariable updated successfully.\n");
    } else {
        printf("Failed to update LargeCustomVariable: %s\n", UA_StatusCode_name(status));
    }
}

void addByteStringVariable(UA_Server *server){
    
    
    UA_NodeId byteStringNodeId = UA_NODEID_STRING(1,"ByteStringVariable");
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "ByteStringVariable");
    attr.dataType = UA_TYPES[UA_TYPES_BYTESTRING].typeId;
    attr.valueRank = -1;


    UA_Variant_setScalar(&attr.value, &byteStringPayloadData, &UA_TYPES[UA_TYPES_BYTESTRING]);

    UA_Server_addVariableNode(server,byteStringNodeId,
                                UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                UA_QUALIFIEDNAME(1, "ByteStringVariable"),
                                UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                attr, NULL, NULL);
}

void addByteStringDataSetField(UA_Server *server){
 /* Add a field to the PublishedDataSet linked to the large variable */
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

//  LargeDataSet
static void
addLargeDataSetField(UA_Server *server) {
    /* Add a field to the PublishedDataSet linked to the large variable */
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("LargeCustomVariable");
    dataSetFieldConfig.field.variable.promotedField = false;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
        UA_NODEID_STRING(1, "LargeCustomVariable");
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
addWriterGroup(UA_Server *server) {
    /* Now we create a new WriterGroupConfig and add the group to the existing
     * PubSubConnection. */
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = 10000;
    writerGroupConfig.writerGroupId = 100;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;

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
addDataSetWriter(UA_Server *server) {
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



/**
 * That's it! You're now publishing the selected fields. Open a packet
 * inspection tool of trust e.g. wireshark and take a look on the outgoing
 * packages. The following graphic figures out the packages created by this
 * tutorial.
 *
 * .. figure:: ua-wireshark-pubsub.png
 *     :figwidth: 100 %
 *     :alt: OPC UA PubSub communication in wireshark
 *
 * The open62541 subscriber API will be released later. If you want to process
 * the the datagrams, take a look on the ``ua_network_pubsub_networkmessage.c``
 * which already contains the decoding code for UADP messages.
 *
 * It follows the main server code, making use of the above definitions. */

static int
run(UA_String *transportProfile, UA_NetworkAddressUrlDataType *networkAddressUrl) {
    /* Create a server */
    UA_Server *server = UA_Server_new();

    /* Add the PubSub components. They are initially disabled */
    addPubSubConnection(server, transportProfile, networkAddressUrl,&pubConnectionIdent);
    addPublishedDataSet(server);
    // addLargeCustomVariable(server); //custom variable
    // addLargeDataSetField(server);   //addingLargeDataSet
    // addDataSetField(server);
    addByteStringVariable(server);
    addByteStringDataSetField(server);
    addWriterGroup(server);
    addDataSetWriter(server);

    UA_NetworkAddressUrlDataType subNetworkAddressUrl =
        {UA_STRING_NULL , UA_STRING("opc.udp://224.0.0.22:4841/")};

    addPubSubConnection(server, transportProfile, &subNetworkAddressUrl, &subConnectionIdent);

    /* Enable the PubSubComponents */
    UA_Server_enableAllPubSubComponents(server);
    
    /* Run the server */
    // UA_StatusCode retval = UA_Server_runUntilInterrupt(server);

    UA_StatusCode retval = UA_Server_run_startup(server);
    UA_Server_run_iterate(server, true);


   
    printf("Servidor rodando.../n");
    clock_t begin, finish;
    begin = clock();
    for(int i=0; i <10; i++){
        //update value
        // updateLargeCustomVariable(server);
        printf("publishing package number %d\n", i);

        //trigger publish
        UA_Server_triggerWriterGroupPublish(server, writerGroupIdent);
    }

    finish = clock();
    double time_spent = (double)(finish - begin) / CLOCKS_PER_SEC;
    printf("duration was %f s\n", time_spent);

    UA_Server_run_shutdown(server);
    /* Delete the server */
    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void
usage(char *progname) {
    printf("usage: %s <uri> [device]\n", progname);
}

int main(int argc, char **argv) {
    UA_Byte customSizeByte[(int)pow(2,max_message_pow)];
    for(int i =0; i < sizeof(customSizeByte); i++){
        customSizeByte[i]='*';
    }
    byteStringPayloadData.length = (int)pow(2,max_message_pow);
    byteStringPayloadData.data = &customSizeByte[0];

    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType networkAddressUrl =
        {UA_STRING_NULL , UA_STRING("opc.udp://224.0.0.22:4840/")};

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strncmp(argv[1], "opc.udp://", 10) == 0) {
            networkAddressUrl.url = UA_STRING(argv[1]);
        } else if (strncmp(argv[1], "opc.eth://", 10) == 0) {
            transportProfile =
                UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-eth-uadp");
            if (argc < 3) {
                printf("Error: UADP/ETH needs an interface name\n");
                return EXIT_FAILURE;
            }
            networkAddressUrl.url = UA_STRING(argv[1]);
        } else {
            printf("Error: unknown URI\n");
            return EXIT_FAILURE;
        }
    }
    if (argc > 2) {
        networkAddressUrl.networkInterface = UA_STRING(argv[2]);
    }

    return run(&transportProfile, &networkAddressUrl);
}
