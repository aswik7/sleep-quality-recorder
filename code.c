#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 100
#define DATE_LEN 12
#define NOTE_LEN 64
#define FNAME "sleeplog.csv"

typedef struct {
    char date[DATE_LEN];    
    double hours; // format hours slept
    int quality;    // from 1-10
    double screen;          // screen hours
    int caffeine;   // mg
    char note[NOTE_LEN];
} SleepEntry;

typedef struct {
    SleepEntry arr[MAX_ENTRIES];
    int count;
} SleepDB;


void read_line(char *buf, int n) {
    if (!fgets(buf, n, stdin)) { buf[0] = '\0'; return; }
    size_t len = strlen(buf);
    if (len && buf[len-1] == '\n') buf[len-1] = '\0';
}

/* File handling (simple CSV) */

int save_db(const SleepDB *db) {
    FILE *f = fopen(FNAME, "w");
    if (!f) { perror("fopen"); return 0; }
    fprintf(f, "date,hours,quality,screen,caffeine,note\n");
    for (int i = 0; i < db->count; ++i) {
        const SleepEntry *e = &db->arr[i];
        // replace commas in note with semicolon
        char note_safe[NOTE_LEN];
        strncpy(note_safe, e->note, NOTE_LEN-1); note_safe[NOTE_LEN-1] = '\0';
        for (char *p = note_safe; *p; ++p) if (*p == ',') *p = ';';
        fprintf(f, "%s,%.2f,%d,%.2f,%d,%s\n",
                e->date, e->hours, e->quality, e->screen, e->caffeine, note_safe);
    }
    fclose(f);
    return 1;
}

int load_db(SleepDB *db) {
    FILE *f = fopen(FNAME, "r");
    if (!f) return 0;
    char line[256];
    // skip header
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    db->count = 0;
    while (db->count < MAX_ENTRIES && fgets(line, sizeof(line), f)) {
        SleepEntry e;
        char note_tmp[NOTE_LEN] = "";
        int scanned = sscanf(line, "%11[^,],%lf,%d,%lf,%d,%63[^\n]",
                             e.date, &e.hours, &e.quality, &e.screen, &e.caffeine, note_tmp);
        if (scanned >= 5) {
            if (scanned == 5) note_tmp[0] = '\0';
            strncpy(e.note, note_tmp, NOTE_LEN-1); e.note[NOTE_LEN-1] = '\0';
            db->arr[db->count++] = e;
        } else break;
    }
    fclose(f);
    return db->count;
}

double compute_score(const SleepEntry *e) {
    double score = 0.0;
    if (e->hours < 8.0) score += (8.0 - e->hours) * 10.0;
    else score += (e->hours - 8.0) * 2.0; // small penalty for big oversleep
    score += (10 - e->quality) * 2.0;
    score += e->screen * 2.0;
    score += (double)e->caffeine / 100.0;
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    return score;
}

/* Average fatigue of last n entries */
double avg_recent_score(const SleepDB *db, int n) {
    if (db->count == 0) return 0.0;
    int used = n < db->count ? n : db->count;
    double sum = 0.0;
    for (int i = db->count - used; i < db->count; ++i) sum += compute_score(&db->arr[i]);
    return sum / used;
}


int predict_risk(const SleepDB *db, double *avg_out) {
    *avg_out = avg_recent_score(db, 3);
    if (*avg_out <= 30) return 0;
    if (*avg_out <= 60) return 1;
    return 2;
}

void add_entry(SleepDB *db) {
    if (db->count >= MAX_ENTRIES) { printf("Storage full (max %d).\n", MAX_ENTRIES); return; }
    SleepEntry e;
    char buf[128];
    printf("Date (YYYY-MM-DD): "); read_line(e.date, DATE_LEN);
    if (e.date[0] == '\0') strncpy(e.date, "unknown", DATE_LEN-1);
    printf("Hours slept (e.g., 7.5): "); read_line(buf, sizeof(buf)); e.hours = atof(buf);
    printf("Sleep quality (1..10): "); read_line(buf, sizeof(buf)); e.quality = atoi(buf);
    if (e.quality < 1) e.quality = 1; if (e.quality > 10) e.quality = 10;
    printf("Screen hours today: "); read_line(buf, sizeof(buf)); e.screen = atof(buf);
    printf("Caffeine mg today (approx): "); read_line(buf, sizeof(buf)); e.caffeine = atoi(buf);
    printf("Note (optional, no commas): "); read_line(e.note, NOTE_LEN);
    db->arr[db->count++] = e;
    printf("Added. Fatigue score: %.2f\n", compute_score(&e));
}

void list_entries(const SleepDB *db) {
    if (db->count == 0) { printf("No entries.\n"); return; }
    for (int i = 0; i < db->count; ++i) {
        const SleepEntry *e = &db->arr[i];
        printf("%2d) %s | %.2f hrs | q=%d | s=%.2fh | c=%d mg | note: %s\n",
               i+1, e->date, e->hours, e->quality, e->screen, e->caffeine, e->note);
    }
}

void show_summary(const SleepDB *db) {
    if (db->count == 0) { printf("No data.\n"); return; }
    double avg8 = 0.0;
    for (int i = 0; i < db->count; ++i) avg8 += db->arr[i].hours;
    avg8 /= db->count;
    printf("Days recorded: %d\n", db->count);
    printf("Average hours slept: %.2f\n", avg8);
    printf("Average recent fatigue (3 days): %.2f\n", avg_recent_score(db, 3));
}

void menu(SleepDB *db) {
    char buf[32];
    while (1) {
        printf("\nSimple Sleep Predictor\n");
        printf("1) Add entry\n2) List entries\n3) Summary\n4) Predict next-day risk\n5) Save\n6) Load\n0) Exit (auto-save)\nChoose: ");
        read_line(buf, sizeof(buf));
        int opt = atoi(buf);
        if (opt == 1) add_entry(db);
        else if (opt == 2) list_entries(db);
        else if (opt == 3) show_summary(db);
        else if (opt == 4) {
            double avg; int r = predict_risk(db, &avg);
            printf("Predicted avg (last 3): %.2f -> ", avg);
            if (r == 0) printf("Low risk\n");
            else if (r == 1) printf("Moderate risk\n");
            else printf("HIGH risk\n");
        }
        else if (opt == 5) { if (save_db(db)) printf("Saved to %s\n", FNAME); else printf("Save failed\n"); }
        else if (opt == 6) {
            int loaded = load_db(db);
            if (loaded) printf("Loaded %d entries from %s\n", loaded, FNAME);
            else printf("Load failed or no file.\n");
        }
        else if (opt == 0) { save_db(db); printf("Auto-saved. Bye.\n"); break; }
        else printf("Invalid option.\n");
    }
}



int main(void) {
    SleepDB db;
    db.count = 0;
    load_db(&db);
    menu(&db);
    return 0;
}
