from peewee import *
from json import JSONEncoder
import json
from collections import OrderedDict

database = MySQLDatabase('dedos', **{'host': '127.0.0.1', 'password': 'root', 'port': 3306, 'user': 'root'})

def dedos_json_dumps(*args, **kwargs):
    return json.dumps(*args, cls=DedosEncoder, **kwargs)

class DedosEncoder(JSONEncoder):
    def default(self, o):
        if '_json_fields' in dir(o):
            return OrderedDict((f,getattr(o, f)) for f in o._json_fields)
        else:
            return o

class UnknownField(object):
    def __init__(self, *_, **__): pass

class BaseModel(Model):
    class Meta:
        database = database

class Msutypes(BaseModel):
    name = CharField()

    class Meta:
        db_table = 'MsuTypes'

    def __repr__(self):
        return '<Msutypes object {{name: {0.name}}}>'.format(self)

    _json_fields = ('name',)

class Runtimes(BaseModel):

    class Meta:
        db_table = 'Runtimes'

    _json_fields = ('id',)

class Threads(BaseModel):
    pk = PrimaryKeyField()
    runtime = ForeignKeyField(db_column='runtime_id', rel_model=Runtimes, to_field='id')
    id = IntegerField(db_column='thread_id')

    _json_fields = ('pk', 'id', 'runtime')

    class Meta:
        db_table = 'Threads'
        indexes = (
            (('thread', 'runtime'), True),
        )

    def __repr__(self):
        return '<Threads object {{id: {0.id}, runtime: {0.runtime.id}}}>'.format(self)

class Msus(BaseModel):
    id = IntegerField(db_column='msu_id', unique=True)
    msu_type = ForeignKeyField(db_column='msu_type_id', rel_model=Msutypes, to_field='id')
    pk = PrimaryKeyField()
    thread = ForeignKeyField(db_column='thread_pk', rel_model=Threads, to_field='pk')

    _json_fields = ('pk', 'id', 'msu_type', 'thread')

    class Meta:
        db_table = 'Msus'

    def __repr__(self):
        return '<Msus object {{id:{0.id}, type:{0.msu_type.name}, rt/th: {0.thread.runtime.id}/{0.thread.id}}}>'.format(self)

class Statistics(BaseModel):
    name = CharField(null=True)

    _json_fields = ('name',)

    class Meta:
        db_table = 'Statistics'

    def __repr__(self):
        return '<Statistics object {{name: {0.name}}}>'.format(self)

class Timeseries(BaseModel):
    msu = ForeignKeyField(db_column='msu_pk', null=True, rel_model=Msus, to_field='pk')
    pk = PrimaryKeyField()
    runtime = ForeignKeyField(db_column='runtime_id', null=True, rel_model=Runtimes, to_field='id')
    statistic = ForeignKeyField(db_column='statistic_id', rel_model=Statistics, to_field='id')
    thread = ForeignKeyField(db_column='thread_pk', null=True, rel_model=Threads, to_field='pk')

    class Meta:
        db_table = 'Timeseries'
        indexes = (
            (('msu_pk', 'statistic'), True),
            (('runtime', 'statistic'), True),
            (('thread_pk', 'statistic'), True),
        )

    def __repr__(self):
        return '<Timeseries object {{msu: {0.msu.id}, stat: {0.statistic.name}, rt/th/msu: {0.runtime.id}/{0.thread.id}/{0.msu.id}>'.format(self)

    _json_fields = ('pk', 'statistic', 'msu', 'runtime', 'thread')

class Points(BaseModel):
    timeseries_pk = ForeignKeyField(db_column='timeseries_pk', rel_model=Timeseries, to_field='pk')
    ts = BigIntegerField(index=True)
    val = DecimalField(null=True)

    class Meta:
        db_table = 'Points'
        indexes = (
            (('timeseries_pk', 'ts'), True),
        )
        primary_key = CompositeKey('timeseries_pk', 'ts')

    _json_fields = ('timeseries_pk',)
