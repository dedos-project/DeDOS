from peewee import *

#FIXME: update to use db_api() self.db var?
database = MySQLDatabase('dedos', **{'host': '192.168.122.252', 'password': 'root', 'user': 'root'})

class UnknownField(object):
    def __init__(self, *_, **__): pass

class BaseModel(Model):
    class Meta:
        database = database

class Msutypes(BaseModel):
    name = CharField(null=True)

    class Meta:
        db_table = 'MsuTypes'

class Runtimes(BaseModel):

    class Meta:
        db_table = 'Runtimes'

class Threads(BaseModel):
    pk = PrimaryKeyField()
    runtime = ForeignKeyField(db_column='runtime_id', rel_model=Runtimes, to_field='id')
    thread = IntegerField(db_column='thread_id')

    class Meta:
        db_table = 'Threads'
        indexes = (
            (('thread', 'runtime'), True),
        )

class Msus(BaseModel):
    msu = IntegerField(db_column='msu_id', unique=True)
    msu_type = ForeignKeyField(db_column='msu_type_id', rel_model=Msutypes, to_field='id')
    pk = PrimaryKeyField()
    thread_pk = ForeignKeyField(db_column='thread_pk', rel_model=Threads, to_field='pk')

    class Meta:
        db_table = 'Msus'

class Statistics(BaseModel):
    name = CharField(null=True)

    class Meta:
        db_table = 'Statistics'

class Timeseries(BaseModel):
    msu_pk = ForeignKeyField(db_column='msu_pk', null=True, rel_model=Msus, to_field='pk')
    pk = PrimaryKeyField()
    runtime = ForeignKeyField(db_column='runtime_id', null=True, rel_model=Runtimes, to_field='id')
    statistic = ForeignKeyField(db_column='statistic_id', rel_model=Statistics, to_field='id')
    thread_pk = ForeignKeyField(db_column='thread_pk', null=True, rel_model=Threads, to_field='pk')

    class Meta:
        db_table = 'Timeseries'
        indexes = (
            (('runtime', 'thread_pk', 'msu_pk', 'statistic'), True),
        )

class Points(BaseModel):
    timeseries_pk = ForeignKeyField(db_column='timeseries_pk', rel_model=Timeseries, to_field='pk')
    ts = BigIntegerField(null=True)
    val = DecimalField(null=True)

    class Meta:
        db_table = 'Points'

