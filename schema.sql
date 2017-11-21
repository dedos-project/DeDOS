DROP TABLE IF EXISTS Points;
DROP TABLE IF EXISTS Timeseries;
DROP TABLE IF EXISTS Msus;
DROP TABLE IF EXISTS MsuTypes;
DROP TABLE IF EXISTS Threads;
DROP TABLE IF EXISTS Runtimes;
DROP TABLE IF EXISTS Statistics;

CREATE TABLE Statistics (
    id int NOT NULL,
    name varchar(64),
    PRIMARY KEY (id)
);

CREATE TABLE Runtimes (
    id  int NOT NULL,
    PRIMARY KEY(id)
);

CREATE TABLE Threads (
    pk  int NOT NULL AUTO_INCREMENT,
    thread_id int NOT NULL,
    runtime_id int NOT NULL,
    FOREIGN KEY (runtime_id) REFERENCES Runtimes(id),
    CONSTRAINT unique_thread_id UNIQUE (thread_id, runtime_id),
    PRIMARY KEY (pk)
);

CREATE TABLE MsuTypes (
    id  int NOT NULL,
    name varchar(64) NOT NULL,
    PRIMARY KEY (id)
);

CREATE TABLE Msus (
    pk  int NOT NULL AUTO_INCREMENT,
    msu_id int NOT NULL,
    msu_type_id int NOT NULL,
    thread_pk int NOT NULL,
    CONSTRAINT unique_msu_id UNIQUE (msu_id),
    FOREIGN KEY (msu_type_id) REFERENCES MsuTypes(id),
    FOREIGN KEY (thread_pk) REFERENCES Threads(pk),
    PRIMARY KEY (pk)
);

CREATE TABLE Timeseries (
    pk  int NOT NULL AUTO_INCREMENT,
    runtime_id int,
    thread_pk int,
    msu_pk int,
    statistic_id int NOT NULL,
    FOREIGN KEY (runtime_id) REFERENCES Runtimes(id),
    FOREIGN KEY (thread_pk) REFERENCES Threads(pk),
    FOREIGN KEY (msu_pk) REFERENCES Msus(pk),
    FOREIGN KEY (statistic_id) REFERENCES Statistics(id),
    CONSTRAINT unique_rt_stat UNIQUE (runtime_id, statistic_id),
    CONSTRAINT unique_msu_stat UNIQUE (msu_pk, statistic_id),
    CONSTRAINT unique_thread_stat UNIQUE (thread_pk, statistic_id),
    PRIMARY KEY (pk)
);

CREATE TABLE Points (
    timeseries_pk int NOT NULL,
    ts BIGINT NOT NULL,
    val DECIMAL(18,9),
    PRIMARY KEY (timeseries_pk, ts),
    INDEX time_index (ts),
    FOREIGN KEY (timeseries_pk) REFERENCES Timeseries(pk)
);
