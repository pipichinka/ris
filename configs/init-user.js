db.auth('admin', 'admin');
db = db.getSiblingDB('database');
db.createUser({
    user: 'user',
    pwd: 'admin',
    roles: [
        {
            role: 'dbOwner',
            db: 'database',
        },
    ],
});