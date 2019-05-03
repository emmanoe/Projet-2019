#ifndef MONITORING_IS_DEF
#define MONITORING_IS_DEF


extern unsigned do_monitoring;

void monitoring_init (int x, int y);
void monitoring_clean ();

void monitoring_begin ();
void monitoring_end ();
void __monitoring_add_tile (int x, int y, int width, int height, int color);

#define monitoring_add_tile(x,y,w,h,c) do { if (do_monitoring) __monitoring_add_tile ((x), (y), (w), (h), (c)); } while(0)

#endif
