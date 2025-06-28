#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#define MAX_LINES 100000
#define INPUT_PUB "publisher_timestamps.csv"
#define INPUT_SUB "subscriber_timestamps.csv"
#define OUTPUT_FILE "latencia_resultado.csv"


#define DATETIME_TO_US(diff) ((diff) / 10)

int main() {
    FILE *fpPub = fopen(INPUT_PUB, "r");
    FILE *fpSub = fopen(INPUT_SUB, "r");
    FILE *fpOut = fopen(OUTPUT_FILE, "w");

    if (!fpPub || !fpSub || !fpOut) {
        perror("Erro ao abrir arquivos");
        return 1;
    }

    char linePub[128], lineSub[128];
    int64_t timestampsPub[MAX_LINES];
    int64_t timestampsSub[MAX_LINES];
    size_t count = 0;


    fgets(linePub, sizeof(linePub), fpPub);
    fgets(lineSub, sizeof(lineSub), fpSub);


    while (fgets(linePub, sizeof(linePub), fpPub) &&
           fgets(lineSub, sizeof(lineSub), fpSub) &&
           count < MAX_LINES) {

        int index1, index2;
        int64_t tPub, tSub;

        if (sscanf(linePub, "%d,%" SCNd64, &index1, &tPub) != 2) break;
        if (sscanf(lineSub, "%d,%" SCNd64, &index2, &tSub) != 2) break;

        timestampsPub[count] = tPub;
        timestampsSub[count] = tSub;
        count++;
    }

    fprintf(fpOut, "Index,LatencyMicroseconds\n");
    int64_t somaLatencia = 0;

    for (size_t i = 0; i < count; i++) {
        int64_t delta = timestampsSub[i] - timestampsPub[i];
        int64_t latency_us = DATETIME_TO_US(delta);
        fprintf(fpOut, "%zu,%" PRId64 "\n", i, latency_us);
        somaLatencia += latency_us;
    }

    double mediaLatencia = (count > 0) ? ((double)somaLatencia / count) : 0.0;

    printf("Mensagens processadas: %zu\n", count);
    printf("Latência média: %.2f microssegundos\n", mediaLatencia);

    fclose(fpPub);
    fclose(fpSub);
    fclose(fpOut);

    return 0;
}
