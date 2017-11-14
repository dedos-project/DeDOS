CREATE TABLE Statistics (
    id   int NOT NULL AUTO_INCREMENT,
    name varchar(64),
    PRIMARY KEY (id)
);

CREATE TABLE Runtimes (
    id  int NOT NULL,
    PRIMARY KEY(id)
);

CREATE TABLE Threads (
    id         int NOT NULL AUTO_INCREMENT,
    thread_id  int NOT NULL,
    runtime_id int NOT NULL,
    FOREIGN KEY (runtime_id) REFERENCES Runtimes(id),
    CONSTRAINT rt_tid UNIQUE (thread_id, runtime_id),
    PRIMARY KEY (id)
);

CREATE TABLE MsuTypes (
    id  int NOT NULL,
    name varchar(64),
    PRIMARY KEY (id)
);

CREATE TABLE Msus (
    id  int NOT NULL AUTO_INCREMENT,
    msu_id int NOT NULL,
    msu_type_id int NOT NULL,
    thread_id int NOT NULL,
    CONSTRAINT msu_id UNIQUE (id),
    FOREIGN KEY (msu_type_id) REFERENCES MsuTypes(id),
    FOREIGN KEY (thread_id) REFERENCES Threads(id),
    PRIMARY KEY (id)
);

CREATE TABLE Timeseries (
    id     int NOT NULL AUTO_INCREMENT,
    runtime_id int,
    thread_id int,
    msu_id int,
    statistic_id int,
    FOREIGN KEY (runtime_id) REFERENCES Runtimes(id),
    FOREIGN KEY (thread_id) REFERENCES Threads(id),
    FOREIGN KEY (msu_id) REFERENCES Msus(id),
    FOREIGN KEY (statistic_id) REFERENCES Statistics(id),
    CONSTRAINT unique_item UNIQUE (runtime_id, thread_id, msu_id, statistic_id),
    PRIMARY KEY (id)
);

CREATE TABLE Points (
    id            int NOT NULL AUTO_INCREMENT,
    timeseries_id int NOT NULL,
    FOREIGN KEY (timeseries_id) REFERENCES Timeseries(id),
    ts bigint,
    val DECIMAL,
    PRIMARY KEY (id)
);

INSERT INTO Statistics (name) VALUES ( "MSU_Q_LEN" );
INSERT INTO Statistics (name) VALUES ( "ITEMS_PROCESSED" );
INSERT INTO Statistics (name) VALUES ( "MSU_EXEC_TIME" );
INSERT INTO Statistics (name) VALUES ( "MSU_IDLE_TIME");
INSERT INTO Statistics (name) VALUES ( "MSU_MEM_ALLOC");
INSERT INTO Statistics (name) VALUES ( "MSU_NUM_STATES");
INSERT INTO Statistics (name) VALUES ( "CTX_SWITCHES");

INSERT INTO MsuTypes VALUES (500, "WS_READ");
INSERT INTO MsuTypes VALUES (501, "SSL_WRITE");
INSERT INTO MsuTypes VALUES (554, "WS_WRITE");
INSERT INTO MsuTypes VALUES (552, "WS_HTTP");
INSERT INTO MsuTypes VALUES (553, "WS_REGEX");
INSERT INTO MsuTypes VALUES (560, "WS_REGEX_ROUTE");
INSERT INTO MsuTypes VALUES (10, "SOCKET");

INSERT INTO Timeseries (runtime_id, statistic_id) (SELECT rt.id, stat.id FROM Runtimes rt JOIN Statistics stat WHERE name = "CPU_Utilization");
INSERT INTO Timeseries (thread_id, statistic_id) (SELECT t.id, stat.id FROM Threads t JOIN Statistics stat WHERE name = "Context_Switches");
INSERT INTO Timeseries (msu_id, statistic_id) (SELECT m.id, stat.id FROM Msus m JOIN Statistics stat WHERE name = "Execution_Time");
INSERT INTO Timeseries (msu_id, statistic_id) (SELECT m.id, stat.id FROM Msus m JOIN Statistics stat WHERE name = "Queue_Length");
