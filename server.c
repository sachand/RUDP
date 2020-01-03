#include "extras.h"
#include "logger.h"
#include "app_settings.h"

#define LOG_TAG "Server"

/**
 * Read .in init file and initialize instance.
 * 
 * @param f                 File pointer to init file
 * @param port_number       Pointer to service port number
 * @param window_size       Pointer to max window size
 * 
 * @return TRUE if parameter initialization successful, FALSE otherwise.
 * */
static boolean
init_specs (FILE *f, uint16_t *port_number, uint32_t *window_size)
{
    if (NULL == f)
    {
        LOGI("No init file found. Going with default init.");
        *port_number = DEFAULT_PORT_NUMBER;
        *window_size = DEFAULT_WINDOW_SIZE_SEND;
        return TRUE;
    }

    fscanf(f, "%hu", port_number);
    if (0 >= *port_number)
    {
        LOGE("Invalid port number: %hu", *port_number);
        return FALSE;
    }

    fscanf(f, "%u", window_size);
    if (0 >= *window_size)
    {
        LOGE("Invalid window size: %u", *window_size);
        return FALSE;
    }

    return TRUE;
}

int
main (int argc, char *argv[])
{
    FILE *f = fopen ("server.in", "r");
    uint16_t port_number;
    uint32_t window_size;

    if (FALSE == init_specs(f, &port_number, &window_size))
    {
        return 0;
    }

    LOGI("Port Number: %hu", port_number);
    LOGI("Window Size: %u", window_size);
    printf("\n");

    if (NULL != f)
    {
        fclose(f);
    }

    // Start serving.
    LOGV("Creating hub");
    endpoint_list list = create_listen_hub(port_number);
    
    LOGV("Managing hub");
    manage_hub(list);
    
    return 0;
}
