/**
 * ============================================================================
 * PROJECT: Queue Management System for Hospitals and General Stores
 * LANGUAGE: Standard C (C11) - Pure Procedural & Struct-Based Modular Design
 * DATA STRUCTURES USED:
 *   1. Priority Queue (Linked List) -> Hospital Triage & Emergency Ordering
 *   2. FIFO Queue (Front/Rear Linked List) -> General Store Waiting Line
 *   3. Singly Linked Lists -> Master Patient & Customer Historical Records
 *   4. Stack (LIFO Linked List) -> Multi-Step Undo Mechanism
 *   5. Arrays -> Multiple Billing Counters & Daily Analytics
 *   6. Linear & Case-Insensitive Substring Search -> ID and Name Lookup
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* ============================================================================
 * CONSTANTS & CONFIGURATION
 * ============================================================================ */
#define MAX_NAME_LEN      64
#define MAX_PHONE_LEN     20
#define MAX_REASON_LEN    100
#define MAX_ITEMS_PER_BILL 20
#define NUM_COUNTERS      3
#define AVG_CONSULT_MINS  12
#define AVG_BILLING_MINS  4

#define HOSPITAL_FILE     "hospital_records.dat"
#define STORE_FILE        "store_records.dat"

/* Record Status Constants */
#define STATUS_WAITING    0
#define STATUS_ACTIVE     1   /* In Consultation / At Billing Counter */
#define STATUS_COMPLETED  2
#define STATUS_CANCELLED  3

/* Hospital Priority Constants (Lower numeric value = Higher priority) */
#define PRIO_EMERGENCY    1
#define PRIO_URGENT       2
#define PRIO_NORMAL       3

/* Undo Action Types */
#define UNDO_HOSP_REGISTER    1
#define UNDO_HOSP_PRIO_CHANGE 2
#define UNDO_STORE_REGISTER   3

/* ============================================================================
 * DATA STRUCTURES DEFINITIONS
 * ============================================================================ */

/* --- 1. HOSPITAL DATA STRUCTURES --- */
typedef struct Patient {
    int id;
    int tokenNumber;
    char name[MAX_NAME_LEN];
    int age;
    char contact[MAX_PHONE_LEN];
    char reason[MAX_REASON_LEN];
    int priority;           /* 1: Emergency, 2: Urgent, 3: Normal */
    int status;             /* WAITING, ACTIVE, COMPLETED, CANCELLED */
    long arrivalOrder;      /* Tie-breaker for strict FIFO within same priority */
    time_t regTime;
    time_t serveTime;
    int waitTimeMinutes;
    struct Patient *next;   /* Pointer for Master Linked List */
} Patient;

/* Node for Hospital Priority Queue */
typedef struct PQNode {
    Patient *patient;
    struct PQNode *next;
} PQNode;

/* --- 2. GENERAL STORE DATA STRUCTURES --- */
typedef struct BillItem {
    char itemName[MAX_NAME_LEN];
    int quantity;
    double unitPrice;
    double subtotal;
} BillItem;

typedef struct Customer {
    int id;
    int tokenNumber;
    char name[MAX_NAME_LEN];
    int declaredItems;
    int status;             /* WAITING, ACTIVE, COMPLETED, CANCELLED */
    int assignedCounter;    /* 1..NUM_COUNTERS or 0 if unassigned */
    BillItem items[MAX_ITEMS_PER_BILL];
    int actualItemCount;
    double totalBill;
    time_t regTime;
    struct Customer *next;  /* Pointer for Master Linked List */
} Customer;

/* Node for General Store FIFO Queue */
typedef struct QNode {
    Customer *customer;
    struct QNode *next;
} QNode;

typedef struct {
    QNode *front;
    QNode *rear;
    int count;
} CustomerQueue;

/* Array Element for Billing Counters */
typedef struct {
    int counterId;
    int isBusy;             /* 0: Available, 1: Busy */
    Customer *activeCustomer;
    int customersServed;
    double revenueCollected;
} BillingCounter;

/* --- 3. UNDO STACK DATA STRUCTURE --- */
typedef struct UndoNode {
    int actionType;
    int targetId;
    int previousValue;      /* Used for restoring previous priority */
    struct UndoNode *next;
} UndoNode;

/* --- 4. SYSTEM STATE & ANALYTICS --- */
typedef struct {
    /* Hospital State */
    Patient *patientMasterHead;
    PQNode *hospPQHead;
    Patient *currentPatient;
    int nextPatientId;
    int nextPatientToken;
    long arrivalSequence;

    /* Store State */
    Customer *customerMasterHead;
    CustomerQueue storeQueue;
    BillingCounter counters[NUM_COUNTERS];
    int nextCustomerId;
    int nextCustomerToken;

    /* Undo Stack */
    UndoNode *undoTop;

    /* Statistics Arrays / Counters */
    int hospStats[4];       /* [0]: Registered, [1]: Served, [2]: Emergency, [3]: Cancelled */
    long totalHospWaitMins;
    int storeStats[3];      /* [0]: Registered, [1]: Served, [2]: Cancelled */
    double totalStoreRevenue;
} SystemContext;

/* ============================================================================
 * UTILITY & INPUT VALIDATION FUNCTIONS
 * ============================================================================ */

void clearInputBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void printSeparator(char ch, int length) {
    for (int i = 0; i < length; i++) putchar(ch);
    putchar('\n');
}

void printHeader(const char *title) {
    printSeparator('=', 78);
    printf("  %s\n", title);
    printSeparator('=', 78);
}

/* Safe integer reader with range checking */
int readInt(const char *prompt, int minVal, int maxVal) {
    int value;
    char term;
    while (1) {
        printf("%s", prompt);
        if (scanf("%d%c", &value, &term) != 2 || term != '\n') {
            printf("  [ERROR] Invalid input! Please enter a valid integer.\n");
            clearInputBuffer();
            continue;
        }
        if (value < minVal || value > maxVal) {
            printf("  [ERROR] Value must be between %d and %d.\n", minVal, maxVal);
            continue;
        }
        return value;
    }
}

/* Safe floating-point reader with minimum value checking */
double readDouble(const char *prompt, double minVal) {
    double value;
    char term;
    while (1) {
        printf("%s", prompt);
        if (scanf("%lf%c", &value, &term) != 2 || term != '\n') {
            printf("  [ERROR] Invalid input! Please enter a valid number.\n");
            clearInputBuffer();
            continue;
        }
        if (value < minVal) {
            printf("  [ERROR] Value cannot be less than %.2f.\n", minVal);
            continue;
        }
        return value;
    }
}

/* Safe non-empty string reader */
void readString(const char *prompt, char *buffer, int maxLen) {
    while (1) {
        printf("%s", prompt);
        if (fgets(buffer, maxLen, stdin) == NULL) {
            clearerr(stdin);
            continue;
        }
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        } else {
            clearInputBuffer();
        }
        /* Trim leading spaces */
        char *start = buffer;
        while (*start == ' ' || *start == '\t') start++;
        if (start != buffer) memmove(buffer, start, strlen(start) + 1);

        if (strlen(buffer) == 0) {
            printf("  [ERROR] Input cannot be empty. Please try again.\n");
            continue;
        }
        return;
    }
}

/* Case-insensitive substring match for search */
int containsIgnoreCase(const char *haystack, const char *needle) {
    char h[MAX_REASON_LEN], n[MAX_REASON_LEN];
    strncpy(h, haystack, sizeof(h) - 1); h[sizeof(h) - 1] = '\0';
    strncpy(n, needle, sizeof(n) - 1);     n[sizeof(n) - 1] = '\0';
    for (int i = 0; h[i]; i++) h[i] = (char)tolower((unsigned char)h[i]);
    for (int i = 0; n[i]; i++) n[i] = (char)tolower((unsigned char)n[i]);
    return strstr(h, n) != NULL;
}

const char* getPriorityLabel(int p) {
    switch (p) {
        case PRIO_EMERGENCY: return "EMERGENCY";
        case PRIO_URGENT:    return "URGENT";
        case PRIO_NORMAL:    return "NORMAL";
        default:             return "UNKNOWN";
    }
}

const char* getStatusLabel(int s) {
    switch (s) {
        case STATUS_WAITING:   return "WAITING";
        case STATUS_ACTIVE:    return "IN-SERVICE";
        case STATUS_COMPLETED: return "COMPLETED";
        case STATUS_CANCELLED: return "CANCELLED";
        default:               return "UNKNOWN";
    }
}

/* ============================================================================
 * UNDO STACK OPERATIONS (LIFO LINKED LIST)
 * ============================================================================ */

void pushUndo(SystemContext *sys, int actionType, int targetId, int prevVal) {
    UndoNode *node = (UndoNode *)malloc(sizeof(UndoNode));
    if (!node) return;
    node->actionType = actionType;
    node->targetId = targetId;
    node->previousValue = prevVal;
    node->next = sys->undoTop;
    sys->undoTop = node;
}

int popUndo(SystemContext *sys, UndoNode *outNode) {
    if (sys->undoTop == NULL) return 0;
    UndoNode *temp = sys->undoTop;
    *outNode = *temp;
    sys->undoTop = temp->next;
    free(temp);
    return 1;
}

/* ============================================================================
 * HOSPITAL PRIORITY QUEUE & MASTER LIST OPERATIONS
 * ============================================================================ */

/* Insert into Master Linked List (Append at tail to preserve chronological history) */
void addPatientToMaster(SystemContext *sys, Patient *p) {
    p->next = NULL;
    if (sys->patientMasterHead == NULL) {
        sys->patientMasterHead = p;
    } else {
        Patient *curr = sys->patientMasterHead;
        while (curr->next != NULL) curr = curr->next;
        curr->next = p;
    }
}

/* Enqueue into Priority Queue: Ordered by priority ASC, then arrivalOrder ASC (FIFO) */
void enqueueHospitalPQ(SystemContext *sys, Patient *p) {
    PQNode *newNode = (PQNode *)malloc(sizeof(PQNode));
    if (!newNode) {
        printf("  [CRITICAL] Memory allocation failed!\n");
        return;
    }
    newNode->patient = p;
    newNode->next = NULL;

    if (sys->hospPQHead == NULL ||
        p->priority < sys->hospPQHead->patient->priority ||
        (p->priority == sys->hospPQHead->patient->priority &&
         p->arrivalOrder < sys->hospPQHead->patient->arrivalOrder)) {
        newNode->next = sys->hospPQHead;
        sys->hospPQHead = newNode;
        return;
    }

    PQNode *curr = sys->hospPQHead;
    while (curr->next != NULL &&
           (curr->next->patient->priority < p->priority ||
           (curr->next->patient->priority == p->priority &&
            curr->next->patient->arrivalOrder < p->arrivalOrder))) {
        curr = curr->next;
    }
    newNode->next = curr->next;
    curr->next = newNode;
}

/* Dequeue highest-priority patient */
Patient* dequeueHospitalPQ(SystemContext *sys) {
    if (sys->hospPQHead == NULL) return NULL;
    PQNode *temp = sys->hospPQHead;
    Patient *p = temp->patient;
    sys->hospPQHead = temp->next;
    free(temp);
    return p;
}

/* Remove a specific patient by ID from Priority Queue (used in cancellation/re-prioritization) */
int removePatientFromPQ(SystemContext *sys, int patientId) {
    PQNode *curr = sys->hospPQHead;
    PQNode *prev = NULL;
    while (curr != NULL) {
        if (curr->patient->id == patientId) {
            if (prev == NULL) sys->hospPQHead = curr->next;
            else prev->next = curr->next;
            free(curr);
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}

Patient* findPatientById(SystemContext *sys, int id) {
    Patient *curr = sys->patientMasterHead;
    while (curr != NULL) {
        if (curr->id == id) return curr;
        curr = curr->next;
    }
    return NULL;
}

int countWaitingPatients(SystemContext *sys) {
    int count = 0;
    PQNode *curr = sys->hospPQHead;
    while (curr != NULL) {
        count++;
        curr = curr->next;
    }
    return count;
}

/* ============================================================================
 * GENERAL STORE FIFO QUEUE & COUNTER OPERATIONS
 * ============================================================================ */

void addCustomerToMaster(SystemContext *sys, Customer *c) {
    c->next = NULL;
    if (sys->customerMasterHead == NULL) {
        sys->customerMasterHead = c;
    } else {
        Customer *curr = sys->customerMasterHead;
        while (curr->next != NULL) curr = curr->next;
        curr->next = c;
    }
}

void enqueueStore(SystemContext *sys, Customer *c) {
    QNode *newNode = (QNode *)malloc(sizeof(QNode));
    if (!newNode) {
        printf("  [CRITICAL] Memory allocation failed!\n");
        return;
    }
    newNode->customer = c;
    newNode->next = NULL;

    if (sys->storeQueue.rear == NULL) {
        sys->storeQueue.front = sys->storeQueue.rear = newNode;
    } else {
        sys->storeQueue.rear->next = newNode;
        sys->storeQueue.rear = newNode;
    }
    sys->storeQueue.count++;
}

Customer* dequeueStore(SystemContext *sys) {
    if (sys->storeQueue.front == NULL) return NULL;
    QNode *temp = sys->storeQueue.front;
    Customer *c = temp->customer;
    sys->storeQueue.front = temp->next;
    if (sys->storeQueue.front == NULL) {
        sys->storeQueue.rear = NULL;
    }
    free(temp);
    sys->storeQueue.count--;
    return c;
}

int removeCustomerFromQueue(SystemContext *sys, int customerId) {
    QNode *curr = sys->storeQueue.front;
    QNode *prev = NULL;
    while (curr != NULL) {
        if (curr->customer->id == customerId) {
            if (prev == NULL) sys->storeQueue.front = curr->next;
            else prev->next = curr->next;

            if (curr == sys->storeQueue.rear) {
                sys->storeQueue.rear = prev;
            }
            free(curr);
            sys->storeQueue.count--;
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }
    return 0;
}

Customer* findCustomerById(SystemContext *sys, int id) {
    Customer *curr = sys->customerMasterHead;
    while (curr != NULL) {
        if (curr->id == id) return curr;
        curr = curr->next;
    }
    return NULL;
}

/* Automatic Counter Assignment Strategy: Lowest-Index Available Counter */
int assignWaitingCustomersToCounters(SystemContext *sys) {
    int assignedCount = 0;
    for (int i = 0; i < NUM_COUNTERS; i++) {
        if (!sys->counters[i].isBusy && sys->storeQueue.front != NULL) {
            Customer *nextCust = dequeueStore(sys);
            if (nextCust != NULL) {
                sys->counters[i].isBusy = 1;
                sys->counters[i].activeCustomer = nextCust;
                nextCust->status = STATUS_ACTIVE;
                nextCust->assignedCounter = sys->counters[i].counterId;
                assignedCount++;
            }
        }
    }
    return assignedCount;
}

/* ============================================================================
 * FILE PERSISTENCE (SAVE & LOAD)
 * ============================================================================ */

void saveSystemData(SystemContext *sys) {
    /* 1. Save Hospital Records */
    FILE *hf = fopen(HOSPITAL_FILE, "wb");
    if (!hf) {
        printf("  [WARNING] Unable to open %s for saving!\n", HOSPITAL_FILE);
    } else {
        fwrite(&sys->nextPatientId, sizeof(int), 1, hf);
        fwrite(&sys->nextPatientToken, sizeof(int), 1, hf);
        fwrite(&sys->arrivalSequence, sizeof(long), 1, hf);
        fwrite(sys->hospStats, sizeof(int), 4, hf);
        fwrite(&sys->totalHospWaitMins, sizeof(long), 1, hf);

        int pCount = 0;
        Patient *p = sys->patientMasterHead;
        while (p) { pCount++; p = p->next; }
        fwrite(&pCount, sizeof(int), 1, hf);

        p = sys->patientMasterHead;
        while (p) {
            fwrite(p, sizeof(Patient), 1, hf);
            p = p->next;
        }
        fclose(hf);
    }

    /* 2. Save Store Records & Counter Stats */
    FILE *sf = fopen(STORE_FILE, "wb");
    if (!sf) {
        printf("  [WARNING] Unable to open %s for saving!\n", STORE_FILE);
    } else {
        fwrite(&sys->nextCustomerId, sizeof(int), 1, sf);
        fwrite(&sys->nextCustomerToken, sizeof(int), 1, sf);
        fwrite(sys->storeStats, sizeof(int), 3, sf);
        fwrite(&sys->totalStoreRevenue, sizeof(double), 1, sf);

        for (int i = 0; i < NUM_COUNTERS; i++) {
            fwrite(&sys->counters[i].customersServed, sizeof(int), 1, sf);
            fwrite(&sys->counters[i].revenueCollected, sizeof(double), 1, sf);
        }

        int cCount = 0;
        Customer *c = sys->customerMasterHead;
        while (c) { cCount++; c = c->next; }
        fwrite(&cCount, sizeof(int), 1, sf);

        c = sys->customerMasterHead;
        while (c) {
            fwrite(c, sizeof(Customer), 1, sf);
            c = c->next;
        }
        fclose(sf);
    }
}

void loadSystemData(SystemContext *sys) {
    /* 1. Load Hospital Records */
    FILE *hf = fopen(HOSPITAL_FILE, "rb");
    if (hf) {
        fread(&sys->nextPatientId, sizeof(int), 1, hf);
        fread(&sys->nextPatientToken, sizeof(int), 1, hf);
        fread(&sys->arrivalSequence, sizeof(long), 1, hf);
        fread(sys->hospStats, sizeof(int), 4, hf);
        fread(&sys->totalHospWaitMins, sizeof(long), 1, hf);

        int pCount = 0;
        if (fread(&pCount, sizeof(int), 1, hf) == 1) {
            for (int i = 0; i < pCount; i++) {
                Patient *p = (Patient *)malloc(sizeof(Patient));
                if (p && fread(p, sizeof(Patient), 1, hf) == 1) {
                    p->next = NULL;
                    addPatientToMaster(sys, p);
                    if (p->status == STATUS_WAITING) {
                        enqueueHospitalPQ(sys, p);
                    } else if (p->status == STATUS_ACTIVE) {
                        sys->currentPatient = p;
                    }
                } else {
                    free(p);
                }
            }
        }
        fclose(hf);
    }

    /* 2. Load Store Records */
    FILE *sf = fopen(STORE_FILE, "rb");
    if (sf) {
        fread(&sys->nextCustomerId, sizeof(int), 1, sf);
        fread(&sys->nextCustomerToken, sizeof(int), 1, sf);
        fread(sys->storeStats, sizeof(int), 3, sf);
        fread(&sys->totalStoreRevenue, sizeof(double), 1, sf);

        for (int i = 0; i < NUM_COUNTERS; i++) {
            fread(&sys->counters[i].customersServed, sizeof(int), 1, sf);
            fread(&sys->counters[i].revenueCollected, sizeof(double), 1, sf);
        }

        int cCount = 0;
        if (fread(&cCount, sizeof(int), 1, sf) == 1) {
            for (int i = 0; i < cCount; i++) {
                Customer *c = (Customer *)malloc(sizeof(Customer));
                if (c && fread(c, sizeof(Customer), 1, sf) == 1) {
                    c->next = NULL;
                    addCustomerToMaster(sys, c);
                    if (c->status == STATUS_WAITING) {
                        enqueueStore(sys, c);
                    } else if (c->status == STATUS_ACTIVE) {
                        int idx = c->assignedCounter - 1;
                        if (idx >= 0 && idx < NUM_COUNTERS && !sys->counters[idx].isBusy) {
                            sys->counters[idx].isBusy = 1;
                            sys->counters[idx].activeCustomer = c;
                        } else {
                            c->status = STATUS_WAITING;
                            c->assignedCounter = 0;
                            enqueueStore(sys, c);
                        }
                    }
                } else {
                    free(c);
                }
            }
        }
        fclose(sf);
    }
}

/* ============================================================================
 * HOSPITAL MODULE CONTROLLERS
 * ============================================================================ */

void registerPatient(SystemContext *sys) {
    printHeader("HOSPITAL: REGISTER NEW PATIENT");
    Patient *p = (Patient *)malloc(sizeof(Patient));
    if (!p) {
        printf("  [ERROR] Memory allocation failed!\n");
        return;
    }

    p->id = sys->nextPatientId++;
    p->tokenNumber = sys->nextPatientToken++;
    readString("  Enter Patient Name       : ", p->name, MAX_NAME_LEN);
    p->age = readInt("  Enter Patient Age (0-120): ", 0, 120);
    readString("  Enter Contact Number     : ", p->contact, MAX_PHONE_LEN);
    readString("  Enter Reason for Visit   : ", p->reason, MAX_REASON_LEN);

    printf("\n  Select Triage Priority Level:\n");
    printf("    1. EMERGENCY (Immediate Life-Threatening)\n");
    printf("    2. URGENT    (Serious, Needs Prompt Care)\n");
    printf("    3. NORMAL    (Routine Consultation)\n");
    p->priority = readInt("  Enter Priority (1-3)     : ", 1, 3);

    p->status = STATUS_WAITING;
    p->arrivalOrder = ++sys->arrivalSequence;
    p->regTime = time(NULL);
    p->serveTime = 0;
    p->waitTimeMinutes = 0;
    p->next = NULL;

    addPatientToMaster(sys, p);
    enqueueHospitalPQ(sys, p);
    pushUndo(sys, UNDO_HOSP_REGISTER, p->id, 0);

    sys->hospStats[0]++; /* Total Registered */
    if (p->priority == PRIO_EMERGENCY) sys->hospStats[2]++; /* Total Emergency */

    int ahead = 0;
    PQNode *curr = sys->hospPQHead;
    while (curr && curr->patient->id != p->id) {
        ahead++;
        curr = curr->next;
    }

    printf("\n  [SUCCESS] Patient Registered!\n");
    printf("  -> Patient ID : P-%d | Token: #H%d | Priority: %s\n",
           p->id, p->tokenNumber, getPriorityLabel(p->priority));
    printf("  -> Queue Position: %d | Estimated Wait: ~%d mins\n",
           ahead + 1, ahead * AVG_CONSULT_MINS);
}

void displayHospitalQueue(SystemContext *sys) {
    printHeader("HOSPITAL: ACTIVE PRIORITY WAITING QUEUE");
    printf("  Current Token in Consultation: ");
    if (sys->currentPatient) {
        printf("#H%d (%s - %s)\n", sys->currentPatient->tokenNumber,
               sys->currentPatient->name, getPriorityLabel(sys->currentPatient->priority));
    } else {
        printf("NONE (Doctor Available)\n");
    }
    printSeparator('-', 78);

    if (sys->hospPQHead == NULL) {
        printf("  [INFO] No patients currently waiting in the queue.\n");
        return;
    }

    /* Visual ASCII Queue Representation */
    printf("  QUEUE FLOW (FRONT -> REAR):\n  ");
    PQNode *vis = sys->hospPQHead;
    while (vis) {
        printf("[#H%d|%c]", vis->patient->tokenNumber, getPriorityLabel(vis->patient->priority)[0]);
        if (vis->next) printf(" -> ");
        vis = vis->next;
    }
    printf("\n");
    printSeparator('-', 78);

    printf("  %-5s %-7s %-8s %-20s %-5s %-11s %-12s\n",
           "Pos", "ID", "Token", "Patient Name", "Age", "Priority", "Est. Wait");
    printSeparator('-', 78);

    PQNode *curr = sys->hospPQHead;
    int pos = 1;
    while (curr != NULL) {
        Patient *p = curr->patient;
        int estWait = (pos - 1) * AVG_CONSULT_MINS;
        printf("  %-5d P-%-5d #H%-6d %-20.20s %-5d %-11s %d mins\n",
               pos, p->id, p->tokenNumber, p->name, p->age,
               getPriorityLabel(p->priority), estWait);
        pos++;
        curr = curr->next;
    }
}

void callNextPatient(SystemContext *sys) {
    printHeader("HOSPITAL: CALL NEXT PATIENT");
    if (sys->currentPatient != NULL) {
        printf("  [WARNING] Patient #H%d (%s) is currently in consultation!\n",
               sys->currentPatient->tokenNumber, sys->currentPatient->name);
        printf("  Please complete the current consultation before calling the next patient.\n");
        return;
    }

    Patient *nextP = dequeueHospitalPQ(sys);
    if (nextP == NULL) {
        printf("  [INFO] Waiting queue is empty. No patients to call.\n");
        return;
    }

    nextP->status = STATUS_ACTIVE;
    nextP->serveTime = time(NULL);
    int elapsedMins = (int)((nextP->serveTime - nextP->regTime) / 60);
    nextP->waitTimeMinutes = (elapsedMins > 0) ? elapsedMins : 1;
    sys->totalHospWaitMins += nextP->waitTimeMinutes;
    sys->currentPatient = nextP;

    printf("  [NOW CALLING] Token #H%d | Patient ID: P-%d\n", nextP->tokenNumber, nextP->id);
    printf("  Name    : %s (Age: %d)\n", nextP->name, nextP->age);
    printf("  Priority: %s | Reason: %s\n", getPriorityLabel(nextP->priority), nextP->reason);
}

void displayCurrentPatient(SystemContext *sys) {
    printHeader("HOSPITAL: CURRENTLY SERVED PATIENT");
    if (sys->currentPatient == NULL) {
        printf("  [INFO] No patient is currently in consultation.\n");
        return;
    }
    Patient *p = sys->currentPatient;
    printf("  Token Number : #H%d\n", p->tokenNumber);
    printf("  Patient ID   : P-%d\n", p->id);
    printf("  Name         : %s (Age: %d)\n", p->name, p->age);
    printf("  Contact      : %s\n", p->contact);
    printf("  Priority     : %s\n", getPriorityLabel(p->priority));
    printf("  Reason       : %s\n", p->reason);
}

void completeConsultation(SystemContext *sys) {
    printHeader("HOSPITAL: COMPLETE CONSULTATION");
    if (sys->currentPatient == NULL) {
        printf("  [ERROR] No active consultation in progress.\n");
        return;
    }
    Patient *p = sys->currentPatient;
    p->status = STATUS_COMPLETED;
    sys->currentPatient = NULL;
    sys->hospStats[1]++; /* Total Served */

    printf("  [SUCCESS] Consultation completed for Patient #H%d (%s).\n",
           p->tokenNumber, p->name);
}

void changePatientPriority(SystemContext *sys) {
    printHeader("HOSPITAL: UPDATE PATIENT PRIORITY");
    if (sys->hospPQHead == NULL) {
        printf("  [INFO] Waiting queue is empty.\n");
        return;
    }
    int id = readInt("  Enter Patient ID (numeric part, e.g., 1001): ", 1, 999999);
    Patient *p = findPatientById(sys, id);
    if (!p) {
        printf("  [ERROR] Patient ID P-%d not found.\n", id);
        return;
    }
    if (p->status != STATUS_WAITING) {
        printf("  [ERROR] Priority can only be changed for WAITING patients (Current: %s).\n",
               getStatusLabel(p->status));
        return;
    }

    printf("  Patient: %s | Current Priority: %s\n", p->name, getPriorityLabel(p->priority));
    int newPrio = readInt("  Enter New Priority (1=Emergency, 2=Urgent, 3=Normal): ", 1, 3);
    if (newPrio == p->priority) {
        printf("  [INFO] Priority is already %s. No changes made.\n", getPriorityLabel(newPrio));
        return;
    }

    int oldPrio = p->priority;
    if (removePatientFromPQ(sys, p->id)) {
        p->priority = newPrio;
        if (newPrio == PRIO_EMERGENCY && oldPrio != PRIO_EMERGENCY) {
            sys->hospStats[2]++;
        }
        enqueueHospitalPQ(sys, p);
        pushUndo(sys, UNDO_HOSP_PRIO_CHANGE, p->id, oldPrio);
        printf("  [SUCCESS] Priority updated from %s to %s without losing patient record.\n",
               getPriorityLabel(oldPrio), getPriorityLabel(newPrio));
    }
}

void cancelPatientRegistration(SystemContext *sys) {
    printHeader("HOSPITAL: CANCEL PATIENT REGISTRATION");
    int id = readInt("  Enter Patient ID to cancel (numeric part): ", 1, 999999);
    Patient *p = findPatientById(sys, id);
    if (!p) {
        printf("  [ERROR] Patient ID P-%d not found.\n", id);
        return;
    }
    if (p->status != STATUS_WAITING) {
        printf("  [ERROR] Cannot cancel! Patient status is currently %s.\n",
               getStatusLabel(p->status));
        return;
    }

    if (removePatientFromPQ(sys, p->id)) {
        p->status = STATUS_CANCELLED;
        sys->hospStats[3]++;
        printf("  [SUCCESS] Registration cancelled for Patient P-%d (%s).\n", p->id, p->name);
    }
}

void searchPatients(SystemContext *sys) {
    printHeader("HOSPITAL: SEARCH PATIENT RECORDS");
    printf("  1. Search by Patient ID\n");
    printf("  2. Search by Patient Name\n");
    int choice = readInt("  Select option (1-2): ", 1, 2);

    int found = 0;
    if (choice == 1) {
        int id = readInt("  Enter Patient ID (numeric part): ", 1, 999999);
        Patient *p = findPatientById(sys, id);
        if (p) {
            printSeparator('-', 78);
            printf("  ID: P-%d | Token: #H%d | Name: %s | Age: %d\n",
                   p->id, p->tokenNumber, p->name, p->age);
            printf("  Priority: %s | Status: %s | Reason: %s\n",
                   getPriorityLabel(p->priority), getStatusLabel(p->status), p->reason);
            found = 1;
        }
    } else {
        char query[MAX_NAME_LEN];
        readString("  Enter Name (or partial name): ", query, MAX_NAME_LEN);
        printSeparator('-', 78);
        Patient *curr = sys->patientMasterHead;
        while (curr) {
            if (containsIgnoreCase(curr->name, query)) {
                printf("  P-%-5d | #H%-5d | %-20.20s | %-10s | %s\n",
                       curr->id, curr->tokenNumber, curr->name,
                       getPriorityLabel(curr->priority), getStatusLabel(curr->status));
                found++;
            }
            curr = curr->next;
        }
    }

    if (!found) printf("  [INFO] No matching patient records found.\n");
}

void displayPatientHistory(SystemContext *sys) {
    printHeader("HOSPITAL: COMPLETE PATIENT HISTORY & RECORDS");
    if (sys->patientMasterHead == NULL) {
        printf("  [INFO] No patient records available.\n");
        return;
    }
    printf("  %-7s %-8s %-20s %-5s %-11s %-12s\n",
           "ID", "Token", "Name", "Age", "Priority", "Status");
    printSeparator('-', 78);

    Patient *curr = sys->patientMasterHead;
    while (curr) {
        printf("  P-%-5d #H%-6d %-20.20s %-5d %-11s %-12s\n",
               curr->id, curr->tokenNumber, curr->name, curr->age,
               getPriorityLabel(curr->priority), getStatusLabel(curr->status));
        curr = curr->next;
    }
    printSeparator('-', 78);
    printf("  SUMMARY -> Registered: %d | Served: %d | Waiting: %d | Cancelled: %d\n",
           sys->hospStats[0], sys->hospStats[1], countWaitingPatients(sys), sys->hospStats[3]);
}

/* ============================================================================
 * GENERAL STORE MODULE CONTROLLERS
 * ============================================================================ */

void registerCustomer(SystemContext *sys) {
    printHeader("GENERAL STORE: REGISTER CUSTOMER");
    Customer *c = (Customer *)malloc(sizeof(Customer));
    if (!c) {
        printf("  [ERROR] Memory allocation failed!\n");
        return;
    }

    c->id = sys->nextCustomerId++;
    c->tokenNumber = sys->nextCustomerToken++;
    readString("  Enter Customer Name          : ", c->name, MAX_NAME_LEN);
    c->declaredItems = readInt("  Enter Number of Items (1-100): ", 1, 100);
    c->status = STATUS_WAITING;
    c->assignedCounter = 0;
    c->actualItemCount = 0;
    c->totalBill = 0.0;
    c->regTime = time(NULL);
    c->next = NULL;

    addCustomerToMaster(sys, c);
    enqueueStore(sys, c);
    pushUndo(sys, UNDO_STORE_REGISTER, c->id, 0);
    sys->storeStats[0]++; /* Total Registered */

    printf("\n  [SUCCESS] Customer Registered! ID: C-%d | Token: #S%d\n",
           c->id, c->tokenNumber);

    /* Automatically assign to a free billing counter if available */
    assignWaitingCustomersToCounters(sys);
    if (c->status == STATUS_ACTIVE) {
        printf("  [AUTO-ASSIGNED] Counter #%d was free! Customer moved directly to Counter #%d.\n",
               c->assignedCounter, c->assignedCounter);
    } else {
        printf("  [QUEUED] All %d counters are currently BUSY. Position in Queue: %d (Est. Wait: ~%d mins)\n",
               NUM_COUNTERS, sys->storeQueue.count,
               (sys->storeQueue.count / NUM_COUNTERS) * AVG_BILLING_MINS);
    }
}

void displayStoreQueueAndCounters(SystemContext *sys) {
    printHeader("GENERAL STORE: BILLING COUNTERS & WAITING QUEUE");

    /* 1. Billing Counters Array Status */
    printf("  BILLING COUNTERS STATUS (Strategy: Lowest-Index Available Assignment):\n");
    printSeparator('-', 78);
    printf("  %-12s %-12s %-25s %-10s %-12s\n",
           "Counter", "Status", "Active Customer", "Served", "Revenue");
    printSeparator('-', 78);
    for (int i = 0; i < NUM_COUNTERS; i++) {
        BillingCounter *bc = &sys->counters[i];
        char activeStr[32] = "None";
        if (bc->isBusy && bc->activeCustomer) {
            snprintf(activeStr, sizeof(activeStr), "#S%d (%s)",
                     bc->activeCustomer->tokenNumber, bc->activeCustomer->name);
        }
        printf("  Counter #%-3d %-12s %-25.25s %-10d $%-11.2f\n",
               bc->counterId, bc->isBusy ? "BUSY" : "AVAILABLE",
               activeStr, bc->customersServed, bc->revenueCollected);
    }
    printSeparator('-', 78);

    /* 2. Waiting Queue Visualization */
    printf("  WAITING QUEUE (%d waiting):\n", sys->storeQueue.count);
    if (sys->storeQueue.front == NULL) {
        printf("  [INFO] No customers currently waiting in line.\n");
        return;
    }

    printf("  FRONT -> ");
    QNode *vis = sys->storeQueue.front;
    while (vis) {
        printf("[#S%d:%s(%d)]", vis->customer->tokenNumber,
               vis->customer->name, vis->customer->declaredItems);
        if (vis->next) printf(" -> ");
        vis = vis->next;
    }
    printf(" <- REAR\n");
}

void callNextCustomerToCounter(SystemContext *sys) {
    printHeader("GENERAL STORE: ASSIGN NEXT CUSTOMER TO COUNTER");
    if (sys->storeQueue.front == NULL) {
        printf("  [INFO] Waiting queue is empty.\n");
        return;
    }
    int assigned = assignWaitingCustomersToCounters(sys);
    if (assigned == 0) {
        printf("  [WARNING] All %d billing counters are currently BUSY!\n", NUM_COUNTERS);
        printf("  Complete billing at an active counter first.\n");
    } else {
        printf("  [SUCCESS] Assigned %d waiting customer(s) to available counter(s).\n", assigned);
    }
}

void processCounterBilling(SystemContext *sys) {
    printHeader("GENERAL STORE: PROCESS BILLING & GENERATE RECEIPT");
    int busyCount = 0;
    for (int i = 0; i < NUM_COUNTERS; i++) {
        if (sys->counters[i].isBusy && sys->counters[i].activeCustomer) {
            printf("  Counter #%d -> Token #S%d | Customer: %s (Est. Items: %d)\n",
                   sys->counters[i].counterId,
                   sys->counters[i].activeCustomer->tokenNumber,
                   sys->counters[i].activeCustomer->name,
                   sys->counters[i].activeCustomer->declaredItems);
            busyCount++;
        }
    }

    if (busyCount == 0) {
        printf("  [INFO] All billing counters are currently idle. No active customer to bill.\n");
        return;
    }

    int cNum = readInt("\n  Select Billing Counter Number to checkout: ", 1, NUM_COUNTERS);
    BillingCounter *bc = &sys->counters[cNum - 1];
    if (!bc->isBusy || bc->activeCustomer == NULL) {
        printf("  [ERROR] Counter #%d is currently AVAILABLE (no active customer).\n", cNum);
        return;
    }

    Customer *cust = bc->activeCustomer;
    int numEntries = readInt("  Enter number of distinct line items to bill (1-20): ", 1, MAX_ITEMS_PER_BILL);

    cust->actualItemCount = numEntries;
    cust->totalBill = 0.0;

    for (int i = 0; i < numEntries; i++) {
        printf("\n  --- Item %d of %d ---\n", i + 1, numEntries);
        readString("  Item Name : ", cust->items[i].itemName, MAX_NAME_LEN);
        cust->items[i].quantity = readInt("  Quantity  : ", 1, 1000);
        cust->items[i].unitPrice = readDouble("  Unit Price: $", 0.01);
        cust->items[i].subtotal = cust->items[i].quantity * cust->items[i].unitPrice;
        cust->totalBill += cust->items[i].subtotal;
    }

    /* Print Formatted Receipt */
    printf("\n");
    printSeparator('*', 60);
    printf("               GENERAL STORE OFFICIAL RECEIPT\n");
    printSeparator('*', 60);
    printf("  Customer ID : C-%d          Token No : #S%d\n", cust->id, cust->tokenNumber);
    printf("  Customer    : %-20s Counter  : #%d\n", cust->name, bc->counterId);
    printSeparator('-', 60);
    printf("  %-24s %6s %12s %12s\n", "Item", "Qty", "Unit Price", "Subtotal");
    printSeparator('-', 60);
    for (int i = 0; i < cust->actualItemCount; i++) {
        printf("  %-24.24s %6d $%11.2f $%11.2f\n",
               cust->items[i].itemName, cust->items[i].quantity,
               cust->items[i].unitPrice, cust->items[i].subtotal);
    }
    printSeparator('-', 60);
    printf("