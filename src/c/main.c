// ── NHL Live Watchface  ·  main.c ─────────────────────────────────────────
#include <pebble.h>

#define KEY_AWAY_ABBR     1
#define KEY_HOME_ABBR     2
#define KEY_AWAY_SCORE    3
#define KEY_HOME_SCORE    4
#define KEY_PERIOD        5
#define KEY_PERIOD_TIME   6
#define KEY_STATUS        10
#define KEY_TEAM_IDX      11
#define KEY_START_TIME    12
#define KEY_AWAY_WINS     13
#define KEY_AWAY_LOSSES   14
#define KEY_HOME_WINS     15
#define KEY_HOME_LOSSES   16
#define KEY_VIBRATE       17
#define KEY_AWAY_SHOTS    18
#define KEY_HOME_SHOTS    19
#define KEY_LAST_GOAL     20
#define KEY_AWAY_SKATERS  21
#define KEY_HOME_SKATERS  22
#define KEY_PENALTY_SECS  23
#define KEY_NEXT_GAME     24
#define KEY_BATTERY_BAR   25
#define KEY_TICKER        26
#define KEY_SERIES_STATUS 27
#define KEY_AWAY_OTL      28
#define KEY_HOME_OTL      29
#define KEY_TZ_OFFSET     31
#define KEY_TICKER_SPEED  32
#define KEY_TEAM_LOGOS    33

#define NUM_TEAMS    32
#define PERSIST_TEAM 1
#define PERSIST_VIB  2
#define PERSIST_BAT  3
#define PERSIST_TZ   4
#define PERSIST_TICKER_SPEED 5
#define PERSIST_TEAM_LOGOS   6

static const char *TEAM_ABBR[NUM_TEAMS] = {
  "ANA","BOS","BUF","CAR","CBJ","CGY","CHI","COL",
  "DAL","DET","EDM","FLA","LAK","MIN","MTL","NJD",
  "NSH","NYI","NYR","OTT","PHI","PIT","SEA","SJS",
  "STL","TBL","TOR","UTA","VAN","VGK","WSH","WPG"
};

static Window    *s_window;
static Layer     *s_canvas;
static Layer     *s_ticker_clip;
static TextLayer *s_ticker_cur;
static TextLayer *s_ticker_next;
static AppTimer  *s_ticker_timer;
static bool       s_anim_running = false;

#define MAX_GAMES 8
#define GAME_LEN  22
static char s_ticker_raw[200];
static char s_games[MAX_GAMES][GAME_LEN];
static int  s_game_count;
static int  s_game_idx;

// Game state
static char s_time_buf[6]     = "00:00";
static char s_date_buf[14]    = "";
static char s_away_abbr[5]    = "---";
static char s_home_abbr[5]    = "---";
static int  s_away_score;
static int  s_home_score;
static int  s_period;
static char s_period_time[8]  = "";
static char s_status[8]       = "off";
static char s_start_time[10]  = "";
static int  s_away_wins;
static int  s_away_losses;
static int  s_away_otl;
static int  s_home_wins;
static int  s_home_losses;
static int  s_home_otl;
static bool s_vibrate         = true;
static int  s_away_shots;
static int  s_home_shots;
static char s_last_goal[20]   = "";
static int  s_away_skaters    = 5;
static int  s_home_skaters    = 5;
static int  s_penalty_secs    = 0;
static char s_next_game[20]   = "";
static int  s_tz_offset       = -5;
static bool s_battery_bar     = true;
static int  s_battery_pct     = 100;
static int  s_team_idx        = 26;  // TOR default
static int  s_prev_score      = -1;
static bool s_i_am_away;
static int  s_ticker_speed    = 5000;
static bool s_team_logos      = true;

// Cached logo bitmaps - reloaded only when the away/home abbreviation changes
static GBitmap *s_away_logo_lg = NULL, *s_away_logo_sm = NULL;
static GBitmap *s_home_logo_lg = NULL, *s_home_logo_sm = NULL;
static char s_away_logo_abbr[5] = "";
static char s_home_logo_abbr[5] = "";

static bool s_goal_flash       = false;
static bool s_goal_away        = false;
static bool s_goal_blink_on    = true;
static AppTimer *s_goal_timer  = NULL;
static AppTimer *s_blink_timer = NULL;
static char s_series[48]       = "";

static GBitmap *s_bmp_stick      = NULL;
static GBitmap *s_bmp_cleaner    = NULL;
static GBitmap *s_bmp_goal[12]   = {NULL};
static int      s_goal_frame      = 0;

static void request_game_data(void);

// ── Ticker ─────────────────────────────────────────────────────────────────
static void ticker_update_text(void) {
  if (!s_ticker_cur) return;
  if (s_game_count == 0) text_layer_set_text(s_ticker_cur, s_date_buf);
  else if (s_game_idx < s_game_count) text_layer_set_text(s_ticker_cur, s_games[s_game_idx]);
}

static void ticker_animation_stopped(Animation *anim, bool finished, void *ctx) {
  int w = layer_get_bounds(s_ticker_clip).size.w;
  int h = 16;
  layer_set_frame(text_layer_get_layer(s_ticker_next), GRect(0, 0, w, h));
  layer_set_frame(text_layer_get_layer(s_ticker_cur),  GRect(0, h, w, h));
  TextLayer *tmp = s_ticker_cur;
  s_ticker_cur   = s_ticker_next;
  s_ticker_next  = tmp;
  s_anim_running = false;
  animation_destroy(anim);
}

static void ticker_advance(void *ctx) {
  if (s_game_count > 1 && s_ticker_cur && s_ticker_next && !s_anim_running) {
    int next_idx   = (s_game_idx + 1) % s_game_count;
    const char *nt = (s_game_count == 0) ? s_date_buf : s_games[next_idx];
    text_layer_set_text(s_ticker_next, nt);
    int w = layer_get_bounds(s_ticker_clip).size.w, h = 16;
    layer_set_frame(text_layer_get_layer(s_ticker_next), GRect(0, h, w, h));
    GRect cf = GRect(0, 0, w, h), ct = GRect(0, -h, w, h);
    GRect nf = GRect(0, h, w, h), nt2= GRect(0,  0, w, h);
    Animation *ac  = (Animation*)property_animation_create_layer_frame(text_layer_get_layer(s_ticker_cur),  &cf, &ct);
    Animation *an  = (Animation*)property_animation_create_layer_frame(text_layer_get_layer(s_ticker_next), &nf, &nt2);
    animation_set_duration(ac, 300); animation_set_duration(an, 300);
    animation_set_curve(ac, AnimationCurveEaseInOut);
    animation_set_curve(an, AnimationCurveEaseInOut);
    animation_set_handlers(an, (AnimationHandlers){ .stopped = ticker_animation_stopped }, NULL);
    Animation *spawn = animation_spawn_create(ac, an, NULL);
    s_anim_running = true;
    animation_schedule(spawn);
    s_game_idx = next_idx;
  }
  s_ticker_timer = app_timer_register((uint32_t)s_ticker_speed, ticker_advance, NULL);
}

static void ticker_parse_and_start(void) {
  s_game_count = 0; s_game_idx = 0;
  int ri = 0, gi = 0, ci = 0;
  while (s_ticker_raw[ri] != '\0' && gi < MAX_GAMES) {
    char ch = s_ticker_raw[ri++];
    if (ch == '|') { s_games[gi][ci] = '\0'; gi++; ci = 0; }
    else if (ci < GAME_LEN - 1) s_games[gi][ci++] = ch;
  }
  if (ci > 0 && gi < MAX_GAMES) { s_games[gi][ci] = '\0'; gi++; }
  s_game_count = gi;
  if (s_ticker_timer) { app_timer_cancel(s_ticker_timer); s_ticker_timer = NULL; }
  s_anim_running = false;
  ticker_update_text();
  if (s_game_count > 1)
    s_ticker_timer = app_timer_register((uint32_t)s_ticker_speed, ticker_advance, NULL);
}

// ── Team colors ────────────────────────────────────────────────────────────
#ifdef PBL_COLOR
static GColor team_color(const char *abbr) {
  if (!abbr) return GColorWhite;
  if (strcmp(abbr,"ANA")==0) return GColorOrange;
  if (strcmp(abbr,"BOS")==0) return GColorYellow;
  if (strcmp(abbr,"BUF")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"CAR")==0) return GColorRed;
  if (strcmp(abbr,"CBJ")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"CGY")==0) return GColorRed;
  if (strcmp(abbr,"CHI")==0) return GColorRed;
  if (strcmp(abbr,"COL")==0) return GColorImperialPurple;
  if (strcmp(abbr,"DAL")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"DET")==0) return GColorRed;
  if (strcmp(abbr,"EDM")==0) return GColorOrange;
  if (strcmp(abbr,"FLA")==0) return GColorRed;
  if (strcmp(abbr,"LAK")==0) return GColorLightGray;
  if (strcmp(abbr,"MIN")==0) return GColorIslamicGreen;
  if (strcmp(abbr,"MTL")==0) return GColorRed;
  if (strcmp(abbr,"NJD")==0) return GColorRed;
  if (strcmp(abbr,"NSH")==0) return GColorYellow;
  if (strcmp(abbr,"NYI")==0) return GColorOrange;
  if (strcmp(abbr,"NYR")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"OTT")==0) return GColorRed;
  if (strcmp(abbr,"PHI")==0) return GColorOrange;
  if (strcmp(abbr,"PIT")==0) return GColorYellow;
  if (strcmp(abbr,"SEA")==0) return GColorTiffanyBlue;
  if (strcmp(abbr,"SJS")==0) return GColorTiffanyBlue;
  if (strcmp(abbr,"STL")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"TBL")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"TOR")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"UTA")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"VAN")==0) return GColorCobaltBlue;
  if (strcmp(abbr,"VGK")==0) return GColorYellow;
  if (strcmp(abbr,"WSH")==0) return GColorRed;
  if (strcmp(abbr,"WPG")==0) return GColorCobaltBlue;
  return GColorWhite;
}

static void draw_team_text(GContext *ctx, const char *text, GFont font, GRect rect,
                           GTextOverflowMode overflow, GTextAlignment align, GColor color) {
  GRect shadow = rect;
  shadow.origin.x += 1;
  shadow.origin.y += 1;
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, text, font, shadow, overflow, align, NULL);
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, rect, overflow, align, NULL);
}
#endif

// ── Team logos (emery + basalt only) ───────────────────────────────────────
// ESPN logos pre-flattened onto opaque black squares at two exact sizes per
// platform (LG for the score row, SM spare) - graphics_draw_bitmap_in_rect()
// clips rather than scales, so draw at native size. Other platforms have no
// logo resources and always fall back to the 3-letter abbreviation.
#if defined(PBL_PLATFORM_EMERY)
  #define HAVE_LOGOS 1
  #define LOGO_LG(a) RESOURCE_ID_LOGO_##a##_EM_LG
  #define LOGO_SM(a) RESOURCE_ID_LOGO_##a##_EM_SM
#elif defined(PBL_PLATFORM_BASALT)
  #define HAVE_LOGOS 1
  #define LOGO_LG(a) RESOURCE_ID_LOGO_##a##_BA_LG
  #define LOGO_SM(a) RESOURCE_ID_LOGO_##a##_BA_SM
#endif

#ifdef HAVE_LOGOS
typedef struct { const char *abbr; uint32_t lg_id; uint32_t sm_id; } LogoEntry;
#define L(a) {#a, LOGO_LG(a), LOGO_SM(a)}
static const LogoEntry LOGO_TABLE[] = {
  L(ANA), L(BOS), L(BUF), L(CAR), L(CBJ), L(CGY), L(CHI), L(COL),
  L(DAL), L(DET), L(EDM), L(FLA), L(LAK), L(MIN), L(MTL), L(NJD),
  L(NSH), L(NYI), L(NYR), L(OTT), L(PHI), L(PIT), L(SEA), L(SJS),
  L(STL), L(TBL), L(TOR), L(UTA), L(VAN), L(VGK), L(WSH), L(WPG),
};
#undef L
#define LOGO_TABLE_LEN (int)(sizeof(LOGO_TABLE)/sizeof(LOGO_TABLE[0]))

static void set_team_logo(GBitmap **bmp_lg, GBitmap **bmp_sm, char *cached_abbr, const char *abbr) {
  if (strcmp(cached_abbr, abbr) == 0) return; // already loaded
  if (*bmp_lg) { gbitmap_destroy(*bmp_lg); *bmp_lg = NULL; }
  if (*bmp_sm) { gbitmap_destroy(*bmp_sm); *bmp_sm = NULL; }
  strncpy(cached_abbr, abbr, 4); cached_abbr[4] = 0;
  for (int i = 0; i < LOGO_TABLE_LEN; i++) {
    if (strcmp(LOGO_TABLE[i].abbr, abbr) == 0) {
      *bmp_lg = gbitmap_create_with_resource(LOGO_TABLE[i].lg_id);
      *bmp_sm = gbitmap_create_with_resource(LOGO_TABLE[i].sm_id);
      return;
    }
  }
}

static void update_team_logos(void) {
  set_team_logo(&s_away_logo_lg, &s_away_logo_sm, s_away_logo_abbr, s_away_abbr);
  set_team_logo(&s_home_logo_lg, &s_home_logo_sm, s_home_logo_abbr, s_home_abbr);
}

static void destroy_team_logos(void) {
  if (s_away_logo_lg) { gbitmap_destroy(s_away_logo_lg); s_away_logo_lg = NULL; }
  if (s_away_logo_sm) { gbitmap_destroy(s_away_logo_sm); s_away_logo_sm = NULL; }
  if (s_home_logo_lg) { gbitmap_destroy(s_home_logo_lg); s_home_logo_lg = NULL; }
  if (s_home_logo_sm) { gbitmap_destroy(s_home_logo_sm); s_home_logo_sm = NULL; }
  s_away_logo_abbr[0] = s_home_logo_abbr[0] = 0;
}
#else
static void update_team_logos(void) { }
static void destroy_team_logos(void) { }
#endif

// Draws a team's logo (if enabled + available) or its 3-letter abbreviation in
// `color`. Logos are drawn at native size, anchored per alignment and centered
// vertically in rect.
static void draw_team_badge(GContext *ctx, const char *abbr, GFont font, GRect rect,
                            GTextAlignment align, GColor color, GBitmap *logo) {
#ifdef HAVE_LOGOS
  if (s_team_logos && logo) {
    GRect b = gbitmap_get_bounds(logo);
    int x = rect.origin.x;
    if (align == GTextAlignmentRight) x = rect.origin.x + rect.size.w - b.size.w;
    int y = rect.origin.y + (rect.size.h - b.size.h) / 2;
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, logo, GRect(x, y, b.size.w, b.size.h));
    return;
  }
#endif
#ifdef PBL_COLOR
  draw_team_text(ctx, abbr, font, rect, GTextOverflowModeTrailingEllipsis, align, color);
#else
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, abbr, font, rect, GTextOverflowModeTrailingEllipsis, align, NULL);
#endif
}

// ── Dots ───────────────────────────────────────────────────────────────────
static void draw_dots(GContext *ctx, int x, int y, int n, int filled) {
  for (int i = 0; i < n; i++) {
    GPoint p = GPoint(x + i * 10, y);
    if (i < filled) {
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_circle(ctx, p, 3);
    } else {
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_circle(ctx, p, 3);
    }
  }
}

// ── Power Play Display ─────────────────────────────────────────────────────
// One line, only while a team is up a skater: [logo] PP 5v4  1:23
static void draw_power_play(GContext *ctx, int x, int y, GFont font, int row_h) {
  if (s_away_skaters == s_home_skaters) return;
  bool away_pp = (s_away_skaters > s_home_skaters);
  const char *abbr = away_pp ? s_away_abbr : s_home_abbr;
  GBitmap *logo = NULL;
#ifdef HAVE_LOGOS
  logo = away_pp ? s_away_logo_sm : s_home_logo_sm;
#endif

  int tx = x;
  if (s_team_logos && logo) {
    GRect lb = gbitmap_get_bounds(logo);
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, logo, GRect(x, y + (row_h - lb.size.h) / 2, lb.size.w, lb.size.h));
    tx = x + lb.size.w + 4;
  } else {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, abbr, font, GRect(x, y, 34, row_h),
      GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    tx = x + 34;
  }

  char pp[12];
#ifdef PBL_PLATFORM_EMERY
  snprintf(pp, sizeof(pp), "PP %dv%d", away_pp ? s_away_skaters : s_home_skaters,
           away_pp ? s_home_skaters : s_away_skaters);
  int pp_w = 62;
#else
  snprintf(pp, sizeof(pp), "PP");
  int pp_w = 20;
#endif
#ifdef PBL_COLOR
  graphics_context_set_text_color(ctx, GColorYellow);
#else
  graphics_context_set_text_color(ctx, GColorWhite);
#endif
  graphics_draw_text(ctx, pp, font, GRect(tx, y + (row_h - 20) / 2, pp_w, 20),
    GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);

  if (s_penalty_secs > 0) {
    char pen[8];
    snprintf(pen, sizeof(pen), "%d:%02d", s_penalty_secs / 60, s_penalty_secs % 60);
    graphics_context_set_text_color(ctx, GColorRed);
    graphics_draw_text(ctx, pen, font, GRect(tx + pp_w, y + (row_h - 20) / 2, 44, 20),
      GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  }
}

// ── Goal Flash ─────────────────────────────────────────────────────────────
static void goal_blink_tick(void *data);

static void goal_flash_stop(void *data) {
  s_goal_flash = false;
  s_goal_frame = 0;
  s_goal_timer = NULL;
  if (s_blink_timer) { app_timer_cancel(s_blink_timer); s_blink_timer = NULL; }
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void goal_blink_tick(void *data) {
  s_goal_frame = (s_goal_frame + 1) % 12;
  s_blink_timer = app_timer_register(70, goal_blink_tick, NULL);
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void start_goal_flash(bool away) {
  s_goal_flash = true;
  s_goal_away = away;
  s_goal_frame = 0;
  if (s_goal_timer)  { app_timer_cancel(s_goal_timer);  s_goal_timer  = NULL; }
  if (s_blink_timer) { app_timer_cancel(s_blink_timer); s_blink_timer = NULL; }
  s_goal_timer  = app_timer_register(10000, goal_flash_stop, NULL);
  s_blink_timer = app_timer_register(500,   goal_blink_tick, NULL);
}

// ── Canvas ─────────────────────────────────────────────────────────────────
static void canvas_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w, h = b.size.h;
  int split = h * 3 / 10;
#ifdef PBL_PLATFORM_EMERY
  split -= 10; // reclaim room below for the taller logo row
#endif
  int by = split + 2;
#ifdef PBL_ROUND
  int hpad = 18;
#else
  int hpad = 2;
#endif

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Battery bar
  if (s_battery_bar) {
    GColor bc = s_battery_pct > 50 ? GColorGreen :
                s_battery_pct > 20 ? GColorYellow : GColorRed;
#ifdef PBL_ROUND
    int32_t arc_start = DEG_TO_TRIGANGLE(120);
    int32_t arc_end   = DEG_TO_TRIGANGLE(240);
    int32_t arc_fill  = arc_start + (arc_end - arc_start) * s_battery_pct / 100;
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, 5, arc_start, arc_end);
    graphics_context_set_fill_color(ctx, bc);
    graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, 5, arc_start, arc_fill);
#else
    int bw = (w * s_battery_pct) / 100;
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(0, h-3, w, 3), 0, GCornerNone);
    graphics_context_set_fill_color(ctx, bc);
    graphics_fill_rect(ctx, GRect(0, h-3, bw, 3), 0, GCornerNone);
#endif
  }

  // Divider
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_draw_line(ctx, GPoint(0, split), GPoint(w, split));

  // Fonts + row positions - same font set as the NFL / MLB watchfaces.
  // score_h matches the LG logo height exactly (36 emery, 26 basalt) and
  // score_y sits clear of the divider, since logos are opaque.
#ifdef PBL_PLATFORM_EMERY
  GFont f_score = fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  GFont f_abbr  = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f_mid   = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f_small = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f_tiny  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  int score_w = 110, abbr_w = 44, score_h = 36;
  int score_y = by + 2, rec_y = by + 40, status_y = by + 56;
  int goal_y = by + 84, sog_y = by + 104;
  int pp_y = by + 128, pp_h = 22;
  int mid_h = 26, small_h = 20;
  GFont f_row = f_small;
#else
  GFont f_score = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f_abbr  = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f_mid   = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f_small = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f_tiny  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  int score_w = 68, abbr_w = 36, score_h = 26;
  int score_y = by, rec_y = by + 26, status_y = by + 38;
  int goal_y = by + 56, sog_y = by + 70;
  int pp_y = by + 88, pp_h = 16;
  int mid_h = 22, small_h = 16;
  GFont f_row = f_small;
#endif

  // Time + date
  graphics_context_set_text_color(ctx, GColorWhite);
#ifdef PBL_PLATFORM_EMERY
  graphics_draw_text(ctx, s_time_buf, f_mid,
    GRect(hpad, 2, 72, 30), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_date_buf, fonts_get_system_font(FONT_KEY_GOTHIC_24),
    GRect(68, 2, w-68-hpad, 26), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
#else
  graphics_draw_text(ctx, s_time_buf, f_mid,
    GRect(hpad, 2, 60, 24), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, s_date_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18),
    GRect(56, 2, w-56-hpad, 20), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
#endif

  // No game
  if (strcmp(s_status, "off") == 0) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "No Game Today", f_mid,
      GRect(hpad, by + 8, w - 2*hpad, mid_h + 4), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    char tl[16];
    snprintf(tl, sizeof(tl), "~ %s ~", TEAM_ABBR[s_team_idx]);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, tl, f_small,
      GRect(0, by + 8 + mid_h + 4, w, small_h), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    if (s_next_game[0]) {
      graphics_context_set_text_color(ctx, GColorLightGray);
      graphics_draw_text(ctx, s_next_game, f_small,
        GRect(hpad, by + 8 + mid_h + 4 + small_h + 6, w - 2*hpad, small_h),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
    return;
  }

  bool live_now = strcmp(s_status, "live") == 0;

  // Badges (logo, or colored abbreviation) + score
#ifdef PBL_COLOR
  GColor away_col = (s_goal_flash && s_goal_away)
    ? (s_goal_blink_on ? GColorChromeYellow : GColorWhite) : team_color(s_away_abbr);
  GColor home_col = (s_goal_flash && !s_goal_away)
    ? (s_goal_blink_on ? GColorChromeYellow : GColorWhite) : team_color(s_home_abbr);
  GColor score_col = (s_goal_flash && s_goal_blink_on) ? GColorChromeYellow : GColorWhite;
#else
  GColor away_col = GColorWhite, home_col = GColorWhite, score_col = GColorWhite;
#endif
  draw_team_badge(ctx, s_away_abbr, f_abbr, GRect(hpad, score_y, abbr_w, score_h),
    GTextAlignmentLeft, away_col, s_away_logo_lg);
  draw_team_badge(ctx, s_home_abbr, f_abbr, GRect(w - abbr_w - hpad, score_y, abbr_w, score_h),
    GTextAlignmentRight, home_col, s_home_logo_lg);
  char sc[16];
  snprintf(sc, sizeof(sc), "%d - %d", s_away_score, s_home_score);
  graphics_context_set_text_color(ctx, score_col);
  graphics_draw_text(ctx, sc, f_score,
    GRect(hpad + abbr_w, score_y, score_w, score_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Records (or playoff series status)
  if (s_series[0]) {
    graphics_context_set_text_color(ctx, GColorChromeYellow);
    graphics_draw_text(ctx, s_series, f_tiny,
      GRect(hpad, rec_y, w-hpad*2, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  } else {
    graphics_context_set_text_color(ctx, GColorLightGray);
    char rec[12];
    snprintf(rec, sizeof(rec), "%d-%d-%d", s_away_wins, s_away_losses, s_away_otl);
    graphics_draw_text(ctx, rec, f_tiny,
      GRect(hpad, rec_y, abbr_w + 20, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    snprintf(rec, sizeof(rec), "%d-%d-%d", s_home_wins, s_home_losses, s_home_otl);
    graphics_draw_text(ctx, rec, f_tiny,
      GRect(w - abbr_w - 20 - hpad, rec_y, abbr_w + 20, 16), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }

  // Period display
  char per[20];
  if (live_now) {
    const char *per_names[] = {"1st","2nd","3rd","OT","SO"};
    int pidx = (s_period >= 1 && s_period <= 5) ? s_period-1 : 0;
    if (s_period_time[0])
      snprintf(per, sizeof(per), "%s %s", per_names[pidx], s_period_time);
    else
      snprintf(per, sizeof(per), "%s", per_names[pidx]);
  } else if (strcmp(s_status, "pre") == 0) {
    snprintf(per, sizeof(per), "%s", s_start_time[0] ? s_start_time : "Pre-Game");
  } else {
    snprintf(per, sizeof(per), "Final");
  }
  graphics_context_set_text_color(ctx, (live_now && s_period==5) ? GColorOrange : GColorYellow);
  graphics_draw_text(ctx, per, f_mid,
    GRect(hpad, status_y, w - 2*hpad, mid_h + 4), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Next game (final)
  if (strcmp(s_status,"final")==0 && s_next_game[0]) {
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, s_next_game, f_row,
      GRect(hpad, goal_y, w - 2*hpad, small_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  if (!live_now) return;

  // Icon rect: animated goal light during flash, otherwise stick+puck / zamboni.
  // Computed up front so text rows that run into it can be narrowed.
  GBitmap *bmp = s_goal_flash ? s_bmp_goal[s_goal_frame]
               : (strcmp(s_period_time, "INT") == 0) ? s_bmp_cleaner : s_bmp_stick;
  GRect icon = GRectZero;
  if (bmp) {
    GRect ib = gbitmap_get_bounds(bmp);
#ifdef PBL_ROUND
    int ix = (w - ib.size.w) / 2;
#else
    int ix = w - ib.size.w - hpad;
#endif
    icon = GRect(ix, h - 3 - 5 - ib.size.h, ib.size.w, ib.size.h);
  }
  int goal_w = (bmp && goal_y + small_h > icon.origin.y) ? icon.origin.x - hpad - 2 : w - 2*hpad;
  int sog_w  = (bmp && sog_y  + small_h > icon.origin.y) ? icon.origin.x - hpad - 2 : w - 2*hpad;

  // Last goal
  if (s_last_goal[0]) {
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s_last_goal, f_row,
      GRect(hpad, goal_y, goal_w, small_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  // Shots on goal
  graphics_context_set_text_color(ctx, GColorMediumAquamarine);
  graphics_draw_text(ctx, "SOG", f_row,
    GRect(hpad, sog_y, 40, small_h), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  char sog[16];
  snprintf(sog, sizeof(sog), "%d | %d", s_away_shots, s_home_shots);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, sog, f_row,
    GRect(hpad + 38, sog_y, sog_w - 38, small_h), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Power play line (only during a power play)
  draw_power_play(ctx, hpad, pp_y, f_row, pp_h);

  if (bmp) {
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    graphics_draw_bitmap_in_rect(ctx, bmp, icon);
  }
}

// ── Clock ──────────────────────────────────────────────────────────────────
static void update_clock(struct tm *t) {
  clock_copy_time_string(s_time_buf, sizeof(s_time_buf));
  strftime(s_date_buf, sizeof(s_date_buf), "%a  %b %d", t);
  if (s_game_count == 0 && s_ticker_cur)
    text_layer_set_text(s_ticker_cur, s_date_buf);
}

static void tick_handler(struct tm *t, TimeUnits u) {
  update_clock(t);
  if (u & MINUTE_UNIT) request_game_data();
  layer_mark_dirty(s_canvas);
}

// ── Inbox ──────────────────────────────────────────────────────────────────
static void inbox_received(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  t = dict_find(iter, KEY_AWAY_ABBR);
  if (t) { strncpy(s_away_abbr, t->value->cstring, 4); s_away_abbr[4]=0; }
  t = dict_find(iter, KEY_HOME_ABBR);
  if (t) { strncpy(s_home_abbr, t->value->cstring, 4); s_home_abbr[4]=0; }
  update_team_logos();
  s_i_am_away = strcmp(s_away_abbr, TEAM_ABBR[s_team_idx])==0;
  int prev_away_score = s_away_score, prev_home_score = s_home_score;
  t = dict_find(iter, KEY_AWAY_SCORE);  if(t) s_away_score  =(int)t->value->int32;
  t = dict_find(iter, KEY_HOME_SCORE);  if(t) s_home_score  =(int)t->value->int32;
  t = dict_find(iter, KEY_PERIOD);      if(t) s_period      =(int)t->value->int32;
  t = dict_find(iter, KEY_PERIOD_TIME);
  if(t){strncpy(s_period_time,t->value->cstring,7);s_period_time[7]=0;}
  t = dict_find(iter, KEY_STATUS);
  if(t){strncpy(s_status,t->value->cstring,7);s_status[7]=0;}
  t = dict_find(iter, KEY_START_TIME);
  if(t){strncpy(s_start_time,t->value->cstring,9);s_start_time[9]=0;}
  t = dict_find(iter, KEY_AWAY_WINS);   if(t) s_away_wins   =(int)t->value->int32;
  t = dict_find(iter, KEY_AWAY_LOSSES); if(t) s_away_losses =(int)t->value->int32;
  t = dict_find(iter, KEY_AWAY_OTL);    if(t) s_away_otl    =(int)t->value->int32;
  t = dict_find(iter, KEY_HOME_WINS);   if(t) s_home_wins   =(int)t->value->int32;
  t = dict_find(iter, KEY_HOME_LOSSES); if(t) s_home_losses =(int)t->value->int32;
  t = dict_find(iter, KEY_HOME_OTL);    if(t) s_home_otl    =(int)t->value->int32;
  t = dict_find(iter, KEY_VIBRATE);
  if(t){s_vibrate=(bool)t->value->int32;persist_write_bool(PERSIST_VIB,s_vibrate);}
  t = dict_find(iter, KEY_AWAY_SHOTS);  if(t) s_away_shots  =(int)t->value->int32;
  t = dict_find(iter, KEY_HOME_SHOTS);  if(t) s_home_shots  =(int)t->value->int32;
  t = dict_find(iter, KEY_LAST_GOAL);
  if(t){strncpy(s_last_goal,t->value->cstring,19);s_last_goal[19]=0;}
  t = dict_find(iter, KEY_AWAY_SKATERS); if(t) s_away_skaters=(int)t->value->int32;
  t = dict_find(iter, KEY_HOME_SKATERS); if(t) s_home_skaters=(int)t->value->int32;
  t = dict_find(iter, KEY_PENALTY_SECS); if(t) s_penalty_secs=(int)t->value->int32;
  t = dict_find(iter, KEY_NEXT_GAME);
  if(t){strncpy(s_next_game,t->value->cstring,19);s_next_game[19]=0;}
  t = dict_find(iter, KEY_BATTERY_BAR);
  if(t){s_battery_bar=(bool)t->value->int32;persist_write_bool(PERSIST_BAT,s_battery_bar);}
  t = dict_find(iter, KEY_TICKER);
  if(t){strncpy(s_ticker_raw,t->value->cstring,199);s_ticker_raw[199]=0;ticker_parse_and_start();}
  t = dict_find(iter, KEY_TZ_OFFSET);
  if(t){s_tz_offset=(int)t->value->int32;persist_write_int(PERSIST_TZ,s_tz_offset);}
  t = dict_find(iter, KEY_TEAM_LOGOS);
  if(t){s_team_logos=(bool)t->value->int32;persist_write_bool(PERSIST_TEAM_LOGOS,s_team_logos);}
  t = dict_find(iter, KEY_TICKER_SPEED);
  if(t){
    int spd=(int)t->value->int32;
    if(spd==5000||spd==10000||spd==30000||spd==60000){
      s_ticker_speed=spd;
      persist_write_int(PERSIST_TICKER_SPEED,s_ticker_speed);
      if(s_ticker_timer){
        app_timer_cancel(s_ticker_timer);
        s_ticker_timer=app_timer_register((uint32_t)s_ticker_speed,ticker_advance,NULL);
      }
    }
  }

  if(strcmp(s_status,"live")==0 && s_vibrate){
    int my=s_i_am_away?s_away_score:s_home_score;
    if(s_prev_score>=0 && my>s_prev_score) vibes_double_pulse();
    s_prev_score=my;
  } else s_prev_score=-1;

  // Goal flash: detect score increase
  if (strcmp(s_status,"live")==0) {
    if (s_away_score > prev_away_score) start_goal_flash(true);
    else if (s_home_score > prev_home_score) start_goal_flash(false);
  }

  t = dict_find(iter, KEY_SERIES_STATUS);
  if(t){strncpy(s_series,t->value->cstring,47);s_series[47]=0;}

  t = dict_find(iter, KEY_TEAM_IDX);
  if(t){
    int idx=(int)t->value->int32;
    if(idx>=0 && idx<NUM_TEAMS && idx!=s_team_idx){
      s_team_idx=idx;
      persist_write_int(PERSIST_TEAM,s_team_idx);
      strncpy(s_away_abbr,"---",4); strncpy(s_home_abbr,"---",4);
      s_away_score=s_home_score=0; s_period=0;
      s_away_wins=s_away_losses=s_away_otl=s_home_wins=s_home_losses=s_home_otl=0;
      s_away_shots=s_home_shots=0; s_away_skaters=s_home_skaters=5;
      s_penalty_secs=0; s_start_time[0]=s_last_goal[0]=s_next_game[0]=0;
      s_prev_score=-1;
      strncpy(s_status,"off",7);
      request_game_data();
    }
  }
  layer_mark_dirty(s_canvas);
}

static void inbox_dropped(AppMessageResult r, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Dropped: %d", (int)r);
}

static void request_game_data(void) {
  DictionaryIterator *iter;
  if(app_message_outbox_begin(&iter)!=APP_MSG_OK) return;
  dict_write_int(iter,KEY_TEAM_IDX,&s_team_idx,sizeof(int),true);
  app_message_outbox_send();
}

static void battery_handler(BatteryChargeState state) {
  s_battery_pct=state.charge_percent;
  if(s_canvas) layer_mark_dirty(s_canvas);
}

// ── Window ─────────────────────────────────────────────────────────────────
static void window_load(Window *window) {
  Layer *root=window_get_root_layer(window);
  GRect bounds=layer_get_bounds(root);
  int w=bounds.size.w;

  s_bmp_stick   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STICK_PUCK);
  s_bmp_cleaner = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ICE_CLEANER);
  static const uint32_t goal_res[12] = {
    RESOURCE_ID_IMAGE_GOAL_0,  RESOURCE_ID_IMAGE_GOAL_1,
    RESOURCE_ID_IMAGE_GOAL_2,  RESOURCE_ID_IMAGE_GOAL_3,
    RESOURCE_ID_IMAGE_GOAL_4,  RESOURCE_ID_IMAGE_GOAL_5,
    RESOURCE_ID_IMAGE_GOAL_6,  RESOURCE_ID_IMAGE_GOAL_7,
    RESOURCE_ID_IMAGE_GOAL_8,  RESOURCE_ID_IMAGE_GOAL_9,
    RESOURCE_ID_IMAGE_GOAL_10, RESOURCE_ID_IMAGE_GOAL_11,
  };
  for (int i = 0; i < 12; i++)
    s_bmp_goal[i] = gbitmap_create_with_resource(goal_res[i]);

  s_canvas=layer_create(bounds);
  layer_set_update_proc(s_canvas,canvas_update);
  layer_add_child(root,s_canvas);

#ifdef PBL_PLATFORM_EMERY
  s_ticker_clip=layer_create(GRect(0,32,w,16));
#else
  s_ticker_clip=layer_create(GRect(0,28,w,16));
#endif
  layer_add_child(root,s_ticker_clip);

  GFont tf=fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_ticker_cur  =text_layer_create(GRect(0, 0,w,16));
  s_ticker_next =text_layer_create(GRect(0,16,w,16));
  TextLayer *tls[2]={s_ticker_cur,s_ticker_next};
  for(int i=0;i<2;i++){
    text_layer_set_background_color(tls[i],GColorBlack);
    text_layer_set_text_color(tls[i],GColorWhite);
    text_layer_set_font(tls[i],tf);
    text_layer_set_overflow_mode(tls[i],GTextOverflowModeTrailingEllipsis);
    text_layer_set_text(tls[i],"");
    layer_add_child(s_ticker_clip,text_layer_get_layer(tls[i]));
  }
}

static void window_unload(Window *window) {
  if(s_goal_timer) {app_timer_cancel(s_goal_timer);  s_goal_timer =NULL;}
  if(s_blink_timer){app_timer_cancel(s_blink_timer); s_blink_timer=NULL;}
  if(s_ticker_timer){app_timer_cancel(s_ticker_timer);s_ticker_timer=NULL;}
  if(s_ticker_cur) {text_layer_destroy(s_ticker_cur); s_ticker_cur=NULL;}
  if(s_ticker_next){text_layer_destroy(s_ticker_next);s_ticker_next=NULL;}
  if(s_ticker_clip){layer_destroy(s_ticker_clip);     s_ticker_clip=NULL;}
  if(s_canvas)     {layer_destroy(s_canvas);          s_canvas=NULL;}
  destroy_team_logos();
  if(s_bmp_stick)  {gbitmap_destroy(s_bmp_stick);     s_bmp_stick=NULL;}
  if(s_bmp_cleaner){gbitmap_destroy(s_bmp_cleaner);   s_bmp_cleaner=NULL;}
  for(int i=0;i<12;i++){if(s_bmp_goal[i]){gbitmap_destroy(s_bmp_goal[i]);s_bmp_goal[i]=NULL;}}
}


static void init(void) {
  s_game_count=0; s_game_idx=0;
  memset(s_ticker_raw,0,sizeof(s_ticker_raw));

  if(persist_exists(PERSIST_TEAM))         s_team_idx    =persist_read_int(PERSIST_TEAM);
  if(persist_exists(PERSIST_VIB))          s_vibrate     =persist_read_bool(PERSIST_VIB);
  if(persist_exists(PERSIST_BAT))          s_battery_bar =persist_read_bool(PERSIST_BAT);
  if(persist_exists(PERSIST_TZ))           s_tz_offset   =persist_read_int(PERSIST_TZ);
  if(persist_exists(PERSIST_TEAM_LOGOS))   s_team_logos  =persist_read_bool(PERSIST_TEAM_LOGOS);
  if(persist_exists(PERSIST_TICKER_SPEED)) s_ticker_speed=persist_read_int(PERSIST_TICKER_SPEED);

  time_t now=time(NULL);
  update_clock(localtime(&now));

  s_window=window_create();
  window_set_background_color(s_window,GColorBlack);
  window_set_window_handlers(s_window,(WindowHandlers){.load=window_load,.unload=window_unload});
  window_stack_push(s_window,true);

  tick_timer_service_subscribe(MINUTE_UNIT,tick_handler);
  battery_state_service_subscribe(battery_handler);
  s_battery_pct=battery_state_service_peek().charge_percent;

  app_message_open(512,64);
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  request_game_data();

}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) { init(); app_event_loop(); deinit(); return 0; }
