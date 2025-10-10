#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERS 3
#define MAX_RESOURCES 3
#define MAX_NAME_LEN 20
#define MAX_ACL_ENTRIES MAX_USERS
#define MAX_CAPABILITIES MAX_RESOURCES


typedef enum {
    READ = 1,    
    WRITE = 2,   
    EXECUTE = 4  
} Permission;

// User and Resource Definitions

typedef struct {
    char name[MAX_NAME_LEN];
} User;

typedef struct {
    char name[MAX_NAME_LEN];
} Resource;

// ACL Entry

typedef struct {
    char username[MAX_NAME_LEN];
    int permissions; 
} ACLEntry;

typedef struct {
    Resource resource;
    ACLEntry entries[MAX_ACL_ENTRIES];
    int entryCount;
} ACLControlledResource;

// Capability Entry

typedef struct {
    char resourceName[MAX_NAME_LEN];
    int permissions; 
} Capability;

typedef struct {
    User user;
    Capability capabilities[MAX_CAPABILITIES];
    int capabilityCount;
} CapabilityUser;

// Utility Functions

void printPermissions(int perm){
    int first = 1;
    if (perm & READ) {
        printf("%sRead", first ? "" : "| ");
        first = 0;
    }
    if (perm & WRITE) {
        printf("%sWrite", first ? "" : "| ");
        first = 0;
    }
    if (perm & EXECUTE) {
        printf("%sExecute", first ? "" : "| ");
        first = 0;
    }
    if (first) printf("None");
}

int hasPermission(int userPerm, int requiredPerm){
    return (userPerm & requiredPerm) == requiredPerm;
}

// ACL System

void checkACLAccess(ACLControlledResource *resList, int resCount,
                    const char *userName, const char *resourceName, int perm){
    
    ACLControlledResource *res = NULL;
    for (int i = 0; i < resCount; i++){
        if (strcmp(resList[i].resource.name, resourceName) == 0){
            res = &resList[i];
            break;
        }
    }

    printf("ACL Check: User %s requests ", userName);
    printPermissions(perm);
    printf(" on %s: ", resourceName);

    if (!res){
        printf("Resource NOT found -> Access DENIED\n");
        return;
    }

    for (int i = 0; i < res->entryCount; i++){
        if (strcmp(res->entries[i].username, userName) == 0){
            int userPerm = res->entries[i].permissions;
            if (hasPermission(userPerm, perm)){
                printf("Access GRANTED (user has ");
                printPermissions(userPerm);
                printf(")\n");
            } else {
                printf("Access DENIED (user has ");
                printPermissions(userPerm);
                printf(")\n");
            }
            return;
        }
    }

    
    printf("User has NO entry for resource -> Access DENIED\n");
}

// Capability System

void checkCapabilityAccess(CapabilityUser *userList, int userCount,
                           const char *userName, const char *resourceName, int perm){
    
    CapabilityUser *u = NULL;
    for (int i = 0; i < userCount; i++){
        if (strcmp(userList[i].user.name, userName) == 0){
            u = &userList[i];
            break;
        }
    }

    printf("Capability Check: User %s requests ", userName);
    printPermissions(perm);
    printf(" on %s: ", resourceName);

    if (!u){
        printf("User NOT found -> Access DENIED\n");
        return;
    }

    for (int i = 0; i < u->capabilityCount; i++){
        if (strcmp(u->capabilities[i].resourceName, resourceName) == 0){
            int capPerm = u->capabilities[i].permissions;
            if (hasPermission(capPerm, perm)){
                printf("Access GRANTED (user has ");
                printPermissions(capPerm);
                printf(")\n");
            } else {
                printf("Access DENIED (user has ");
                printPermissions(capPerm);
                printf(")\n");
            }
            return;
        }
    }

    
    printf("User has NO capability for resource -> Access DENIED\n");
}

int main(){

    // Sample users and resources

    User users[MAX_USERS] = {{"Alice"}, {"Bob"}, {"Charlie"}};
    Resource resources[MAX_RESOURCES] = {{"File1"}, {"File2"}, {"File3"}};

    
    ACLControlledResource aclResources[MAX_RESOURCES];
    
    for (int i = 0; i < MAX_RESOURCES; i++){
        strncpy(aclResources[i].resource.name, resources[i].name, MAX_NAME_LEN-1);
        aclResources[i].resource.name[MAX_NAME_LEN-1] = '\0';
        aclResources[i].entryCount = 0;
    }
    
    strncpy(aclResources[0].entries[aclResources[0].entryCount].username, "Alice", MAX_NAME_LEN-1);
    aclResources[0].entries[aclResources[0].entryCount].permissions = READ | WRITE;
    aclResources[0].entryCount++;
    strncpy(aclResources[0].entries[aclResources[0].entryCount].username, "Bob", MAX_NAME_LEN-1);
    aclResources[0].entries[aclResources[0].entryCount].permissions = READ;
    aclResources[0].entryCount++;

    
    strncpy(aclResources[1].entries[aclResources[1].entryCount].username, "Alice", MAX_NAME_LEN-1);
    aclResources[1].entries[aclResources[1].entryCount].permissions = READ;
    aclResources[1].entryCount++;
    strncpy(aclResources[1].entries[aclResources[1].entryCount].username, "Charlie", MAX_NAME_LEN-1);
    aclResources[1].entries[aclResources[1].entryCount].permissions = READ | EXECUTE;
    aclResources[1].entryCount++;

    
    strncpy(aclResources[2].entries[aclResources[2].entryCount].username, "Bob", MAX_NAME_LEN-1);
    aclResources[2].entries[aclResources[2].entryCount].permissions = WRITE;
    aclResources[2].entryCount++;
    strncpy(aclResources[2].entries[aclResources[2].entryCount].username, "Charlie", MAX_NAME_LEN-1);
    aclResources[2].entries[aclResources[2].entryCount].permissions = READ | WRITE | EXECUTE;
    aclResources[2].entryCount++;

    // Capability Setup

    CapabilityUser capUsers[MAX_USERS];
    
    for (int i = 0; i < MAX_USERS; i++){
        strncpy(capUsers[i].user.name, users[i].name, MAX_NAME_LEN-1);
        capUsers[i].user.name[MAX_NAME_LEN-1] = '\0';
        capUsers[i].capabilityCount = 0;
    }
    
    strncpy(capUsers[0].capabilities[capUsers[0].capabilityCount].resourceName, "File1", MAX_NAME_LEN-1);
    capUsers[0].capabilities[capUsers[0].capabilityCount].permissions = READ | WRITE;
    capUsers[0].capabilityCount++;
    strncpy(capUsers[0].capabilities[capUsers[0].capabilityCount].resourceName, "File2", MAX_NAME_LEN-1);
    capUsers[0].capabilities[capUsers[0].capabilityCount].permissions = READ;
    capUsers[0].capabilityCount++;

    
    strncpy(capUsers[1].capabilities[capUsers[1].capabilityCount].resourceName, "File1", MAX_NAME_LEN-1);
    capUsers[1].capabilities[capUsers[1].capabilityCount].permissions = READ;
    capUsers[1].capabilityCount++;
    strncpy(capUsers[1].capabilities[capUsers[1].capabilityCount].resourceName, "File3", MAX_NAME_LEN-1);
    capUsers[1].capabilities[capUsers[1].capabilityCount].permissions = WRITE;
    capUsers[1].capabilityCount++;

    
    strncpy(capUsers[2].capabilities[capUsers[2].capabilityCount].resourceName, "File2", MAX_NAME_LEN-1);
    capUsers[2].capabilities[capUsers[2].capabilityCount].permissions = READ | EXECUTE;
    capUsers[2].capabilityCount++;
    strncpy(capUsers[2].capabilities[capUsers[2].capabilityCount].resourceName, "File3", MAX_NAME_LEN-1);
    capUsers[2].capabilities[capUsers[2].capabilityCount].permissions = READ | WRITE | EXECUTE;
    capUsers[2].capabilityCount++;

    // Test ACL

    checkACLAccess(aclResources, MAX_RESOURCES, "Alice", "File1", READ);     //  GRANTED
    checkACLAccess(aclResources, MAX_RESOURCES, "Bob", "File1", WRITE);      //  DENIED
    checkACLAccess(aclResources, MAX_RESOURCES, "Charlie", "File1", READ);   //  DENIED

    // Test Capability

    checkCapabilityAccess(capUsers, MAX_USERS, "Alice", "File1", WRITE);     //  GRANTED
    checkCapabilityAccess(capUsers, MAX_USERS, "Bob", "File1", WRITE);       //  DENIED
    checkCapabilityAccess(capUsers, MAX_USERS, "Charlie", "File2", WRITE);   //  DENIED 

    
    printf("\n-- Additional tests --\n");

    checkACLAccess(aclResources, MAX_RESOURCES, "Charlie", "File3", READ | WRITE);
    checkCapabilityAccess(capUsers, MAX_USERS, "Charlie", "File3", READ | WRITE);

    
    checkACLAccess(aclResources, MAX_RESOURCES, "Bob", "File3", READ);
    checkCapabilityAccess(capUsers, MAX_USERS, "Bob", "File3", READ);

    return 0;
}
