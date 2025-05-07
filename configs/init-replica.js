db.auth('admin', 'admin');
rs.initiate(
    {_id: "rs1", version: 1,
        members: [
            { _id: 0, host : "mongodb_1:27017", priority: 2 },
            { _id: 1, host : "mongodb_2:27017", priority: 1 },
            { _id: 2, host : "mongodb_3:27017", priority: 1 }
        ]
    }
);