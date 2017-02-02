#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <regex.h>

#include <glib.h>

#include <bot_core/bot_core.h>
#include <lcm/lcm.h>

typedef struct _state_t state_t;


static void
usage (int argc, char ** argv)
{
    fprintf (stderr, "Usage %s -c <IMG_CHAN> [OPTIONS] <source_logfile>\n"
             "\n"
             "Generate jpeg images corresponding to LCM messages with <IMG_CHAN> channel name \n"
             "located within specified LCM logfile with generated files named according to image utime.\n"
             "\n"
             " -h                      prints this help and exits\n"
             " -n, --numframe          name images according to frame number starting at 0\n" 
             " -c, --channel=IMG_CHAN  channel name for desired image\n"
             " -s, --start=START       start time. Images logged less than START seconds\n"
             "                         after the first logged message will be ignored.\n"
             " -e, --end=END           end time. Images logged after END seconds\n"
             "                         from the beginning of the log will be ignored.\n"
             " -v, --verbose           verbose output\n"
             , argv[0]);
}
             
int main (int argc, char **argv)
{
    int verbose = 0;
    //char *pattern = NULL;
    char *channame = NULL;
    char *lcm_fname = NULL;
    int64_t start_utime = 0;
    int64_t end_utime = -1;
    int have_end_utime = 0;
    int use_framenum_name = 0;

    char *optstring = "hnvc:s:e:";
    char c;
    struct option long_opts[] = {
        { "help", no_argument, NULL, 'h' },
        { "verbose", no_argument, NULL, 'v' },
        { "numframe", no_argument, NULL, 'n' },
        { "channel", required_argument, NULL, 'c' },
        { "start", required_argument, NULL, 's' },
        { "end", required_argument, NULL, 'e' },
        { 0, 0, 0, 0 }
    };

    while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0)
    {
        switch (c) {
            case 's':
                {
                    char *eptr = NULL;
                    double start_time = strtod (optarg, &eptr);
                    if (*eptr != 0)
                        usage (argc, argv);
                    start_utime = (int64_t) (start_time * 1000000);
                }
                break;
            case 'e':
                {
                    char *eptr = NULL;
                    double end_time = strtod (optarg, &eptr);
                    if (*eptr != 0)
                        usage (argc, argv);
                    end_utime = (int64_t) (end_time * 1000000);
                    have_end_utime = 1;
                }
                break;
            case 'c':
                channame = strdup (optarg);
                break;
            case 'n':
                use_framenum_name = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
            default:
                usage (argc, argv);
            return 1;
        }
    }

    if (start_utime < 0 || (have_end_utime && (end_utime < start_utime)))
        usage (argc, argv);

    if (!channame)
        usage (argc, argv);
    
    lcm_fname = argv[argc -1];

    // now, generate the images
    lcm_eventlog_t *src_log = lcm_eventlog_create (lcm_fname, "r");
    if (!src_log) {
        fprintf (stderr, "Unable to open source logfile %s\n", lcm_fname);
        return 1;
    }

    int64_t first_event_timestamp;
    int have_first_event_timestamp = 0;
    int frameno = 0;
    for (lcm_eventlog_event_t *event = lcm_eventlog_read_next_event (src_log);
         event != NULL;
         event = lcm_eventlog_read_next_event (src_log)) {
        
        if (have_first_event_timestamp == 0) {
            first_event_timestamp = event->timestamp;
            have_first_event_timestamp = 1;
        }

        int64_t elapsed = event->timestamp - first_event_timestamp;
        
        // proceed with the next event in the log if we have yet
        // to reach the desired start time
        if (elapsed < start_utime) {
            lcm_eventlog_free_event (event);
            continue;
        }

        // have we passed the desired end time?
        if (have_end_utime && elapsed > end_utime) {
            lcm_eventlog_free_event (event);
            break;
        }

        // does the channel name match the specified regex?
        int decode_status;
        if (strcmp (channame, event->channel) == 0) {

            bot_core_image_t img;
            memset (&img, 0, sizeof (bot_core_image_t));
            decode_status =  bot_core_image_t_decode (event->data, 0, event->datalen, &img);
            if (decode_status < 0)
                fprintf (stderr, "Error %d decoding message\n", decode_status);
            else {
                char *out_fname;
                if (use_framenum_name)
                    out_fname = g_strdup_printf ("%05d.jpg", frameno);
                else
                    out_fname = g_strdup_printf ("%"PRId64".jpg", img.utime);
                    

                FILE *fp = fopen (out_fname, "w");
                if (!fp) 
                    fprintf (stderr, "Error creating jpeg file\n");
                else {
                    fwrite (img.data, img.size, 1, fp);
                    fclose (fp);
                    free (out_fname);
                    frameno += 1;
                }
            }
                         
            bot_core_image_t_decode_cleanup (&img);
        }
        lcm_eventlog_free_event (event);
    }
    
    lcm_eventlog_destroy (src_log);

    if (verbose)
        fprintf (stdout, "Generated %d images\n", frameno);
    
    
    return 0;
}
