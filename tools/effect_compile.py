"""Lower the supplied effect language to ordinary Lacooda instructions.

Source names live in the generated binding manifest, never in the ISA.
Unknown constructs are errors. No generic effect/action dispatch is emitted.
"""
from contextlib import contextmanager
from dataclasses import dataclass
from fractions import Fraction
import re
from effect_source import Call, Node

@dataclass
class Ref:
    operand: str
    kind: str = 'value'  # value, address, collection

class Schema:
    def __init__(self):
        self.fields={};self.enums={};self.events={};self.zones={};self.warnings=[]
    def field(self,name):
        if name not in self.fields:self.fields[name]=len(self.fields)+1
        if self.fields[name]>4095:raise ValueError('attribute width exhausted')
        return '#'+str(self.fields[name])
    def enum(self,domain,name):
        values=self.enums.setdefault(domain,{})
        name=str(name)
        if name not in values:values[name]=len(values)+1
        return '#'+str(values[name])
    def event(self,name):
        if name not in self.events:self.events[name]=128+len(self.events)
        if self.events[name]>4095:raise ValueError('event subcode width exhausted')
        return '#'+str(self.events[name])
    def zone(self,name):
        if name not in self.zones:self.zones[name]=len(self.zones)+1
        if self.zones[name]>63:raise ValueError('zone width exhausted')
        return self.zones[name]

class Compiler:
    def __init__(self,effect,schema):
        self.effect=effect;self.schema=schema;self.code=[];self.labels={};self.label_index=0
        self.used={'V':{0,1},'A':{0,1},'F':set()};self.scopes=[[]]
        self.vars={'self':Ref('A0','address')};self.inputs={'self':self.vars['self'],'event_record':Ref('A1','address'),'controller':Ref('V0'),'opponent':Ref('V1')}
        self.last=None;self.line=effect.line;self.cost=False;self.exit=self.label();self.source_lines=set();self.warnings=[];self.cleanups=[];self.ephemeral=[]
    def label(self):
        self.label_index+=1;return 'L'+str(self.label_index)
    def mark(self,label):
        if label in self.labels:raise ValueError('duplicate generated label')
        self.labels[label]=len(self.code)
    def emit(self,op,*args,meaning=None,flags=0,cause=0):
        args=[x.operand if isinstance(x,Ref) else str(x) for x in args]
        self.code.append((op,args,self.line,meaning or self.meaning(op),flags,cause))
    def meaning(self,op):
        return {'LOAD':'Read state.','STORE':'Write state.','ATTRIBUTE':'Address a property.','ENUMERATE':'Snapshot candidate objects.','AT':'Read a collection element.','APPEND':'Retain this object.','LENGTH':'Count collected objects.','COMPARE':'Evaluate the comparison.','JUMPIF':'Branch on the comparison.','JUMP':'Continue at the target instruction.','CHOOSE':'Wait for a player decision.','RANDOM':'Generate a seeded result.','MOVE':'Move the selected object.','MODIFY':'Install a numeric modifier.','UNMODIFY':'Remove this modifier.','SUBSCRIBE':'Capture a body for matching events.','CANCEL':'Unregister the captured body.','EVENT':'Record the native event.','HALT':'End this invocation.','ALU':'Calculate the value.','SET':'Retain the value.','SELECT':'Retain the address.','SWAP':'Exchange the values.','SUMMON':'Summon the specified object.','HISTORY':'Read the event history.'}.get(op,'Perform the native operation.')
    def alloc(self,kind='value',permanent=False):
        bank='A' if kind=='address' else 'F' if kind=='flag' else 'V'
        for n in range(256):
            if n not in self.used[bank]:
                self.used[bank].add(n)
                if not permanent:self.scopes[-1].append((bank,n))
                return Ref(bank+str(n),kind)
        raise ValueError(f'{bank} register allocation exhausted at line {self.line}')
    @contextmanager
    def scope(self):
        self.scopes.append([])
        try:yield
        finally:
            for bank,n in self.scopes.pop():self.used[bank].remove(n)
    def bind(self,name,value):
        if name not in self.vars:self.vars[name]=self.alloc(value.kind,True)
        target=self.vars[name]
        if target.kind!=value.kind:raise ValueError(f'inconsistent type for {name}: {target.kind}/{value.kind}')
        if target.operand!=value.operand:self.emit('SET',target,value)
        return target
    def warning(self,message):
        item={'line':self.line,'message':message}
        if item not in self.warnings:self.warnings.append(item)
    def number(self,x):
        return Ref('#'+str(int(str(x).lstrip('#'))))
    def compare(self,a,b,method='EQUAL'):
        f=self.alloc('flag');self.emit('COMPARE',f,a,b,method);return f
    def arithmetic(self,a,b,method):
        v=self.alloc();self.emit('ALU',v,a,b,method);return v
    def guard(self,flag):self.emit('JUMPIF',flag,self.exit,'FALSE')
    def address(self,x):
        if isinstance(x,Ref):
            if x.kind=='address':return x
            if x.kind=='collection':
                size=self.length(x);self.guard(self.compare(size,Ref('#0'),'GREATER'));a=self.alloc('address');self.emit('AT',a,x,'#0');return a
            a=self.alloc('address');self.emit('SET',a,x);return a
        if isinstance(x,Call):
            if x.name in ('REF','R'):return self.address(x.args[0])
            if x.name=='PERSIST':return self.load(Ref('A0','address'),'PERSIST.'+str(x.args[0]),'address')
            if x.name=='LINKED':return self.load(self.address(x.args[1]),'LINK.'+str(x.args[0]),'address')
            if x.name=='EVENT':return self.address('event.'+str(x.args[0]))
            if x.name=='QUERY':return self.address(self.query(x.args))
        if isinstance(x,dict) and 'controller_of'in x:return self.player_address(self.player(x))
        if isinstance(x,str):
            if x=='DUEL':return Ref('[0]','address')
            if x in ('PLAYER','CONTROLLER','OPPONENT','NON_TURN_PLAYER','TURN_PLAYER'):return self.player_address(self.player(x))
            if x in self.vars:return self.address(self.vars[x])
            if x=='equipped_monster':return self.load(Ref('A0','address'),'EQUIP_TARGET','address')
            if x.startswith('event.'):
                return self.load(Ref('A1','address'),x[6:].upper(),'address')
            if x=='remaining_monster':return self.address(self.query({'controller':'ANY','zones':['MONSTER']}))
            result=self.alloc('address',True);self.vars[x]=result;self.inputs[x]=result
            self.warning('Source reference is not defined in this block; declared address input: '+x)
            return result
        raise ValueError('cannot resolve object '+repr(x))
    def player(self,x):
        if isinstance(x,Ref):return x
        if x in ('PLAYER','CONTROLLER') if isinstance(x,str) else False:return Ref('V0')
        if x=='OPPONENT':return Ref('V1')
        if x=='TURN_PLAYER':return self.load(Ref('[0]','address'),'TURN_PLAYER')
        if x=='NON_TURN_PLAYER':return self.arithmetic(Ref('#1'),self.load(Ref('[0]','address'),'TURN_PLAYER'),'SUBTRACT')
        if isinstance(x,dict) and 'controller_of'in x:return self.load(self.address(x['controller_of']),'CONTROLLER')
        if isinstance(x,Call):
            if x.name=='EVENT':return self.value('event.'+str(x.args[0]))
            if x.name=='REF':return self.value(x.args[0])
        return self.value(x)
    def player_address(self,player):
        a=self.alloc('address');end=self.label();self.emit('SELECT',a,'[0:0]');f=self.compare(player,Ref('#0'));self.emit('JUMPIF',f,end);self.emit('SELECT',a,'[1:0]');self.mark(end);return a
    def zone_address(self,player,zone):
        z=self.schema.zone(zone);a=self.alloc('address');end=self.label()
        self.emit('SELECT',a,f'[0:{z}:0]');self.emit('JUMPIF',self.compare(player,Ref('#0')),end);self.emit('SELECT',a,f'[1:{z}:0]');self.mark(end);return a
    def field(self,subject,name):
        out=self.alloc('address');self.emit('ATTRIBUTE',out,subject,self.schema.field(name),meaning='Address property '+name+'.');return out
    def load(self,subject,name,kind='value'):
        out=self.alloc(kind);self.emit('LOAD',out,self.field(subject,name));return out
    def store(self,subject,name,value):self.emit('STORE',self.field(subject,name),value)
    def value(self,x,domain=None):
        if isinstance(x,Ref):return x
        if isinstance(x,dict):
            if 'calc'in x:
                c=x['calc'];args=[self.value(a) for a in c['args']];v=args[0]
                for b in args[1:]:v=self.arithmetic(v,b,{'MUL':'MULTIPLY','DIV':'DIVIDE','ADD':'ADD','SUB':'SUBTRACT','MIN':'MINIMUM','MAX':'MAXIMUM'}[c['op']])
                return v
            if 'event_value'in x:return self.load(Ref('A1','address'),str(x['event_value']).upper())
            if 'same_as'in x:return self.load(self.address(x['same_as']),domain or 'NAME')
            if 'from_store'in x:return self.value(Call('R',[x['from_store']]))
            if 'history_count'in x:return self.history_count(x['history_count'])
            if 'controller_of'in x:return self.player(x)
            raise ValueError('unknown value object '+repr(x))
        if isinstance(x,Call):
            if x.name in ('R','REF'):
                name=x.args[0]
                if name in self.vars:return self.vars[name]
                if x.name=='REF':return self.address(name)
                result=self.alloc('value',True);self.vars[name]=result;self.inputs[name]=result
                self.warning('Source value is not defined in this block; declared numeric input: '+name);return result
            if x.name=='PROP':return self.load(self.address(x.args[0]),str(x.args[1]))
            if x.name=='COUNT':return self.length(self.collection(x.args[0]))
            if x.name=='PERSIST':return self.load(Ref('A0','address'),'PERSIST.'+str(x.args[0]))
            if x.name=='EVENT':return self.value('event.'+str(x.args[0]))
        if isinstance(x,str):
            if re.fullmatch(r'#?-?\d+',x):return self.number(x)
            if x in self.vars:return self.vars[x]
            if x in ('PLAYER','CONTROLLER','OPPONENT','NON_TURN_PLAYER','TURN_PLAYER'):return self.player(x)
            if x.startswith('event.'):
                parts=x[6:].split('.')
                if len(parts)==1:return self.load(Ref('A1','address'),parts[0].upper())
                return self.load(self.address('event.'+parts[0]),parts[1].upper())
            if '.'in x and domain is None:
                obj,field=x.rsplit('.',1);return self.load(self.address(obj),field.upper())
            if domain:return Ref(self.schema.enum(domain,x))
        raise ValueError('unknown value '+repr(x))
    def empty(self):
        v=self.alloc('collection');self.emit('ENUMERATE',v,'NONE');return v
    def length(self,items):
        v=self.alloc();self.emit('LENGTH',v,items);return v
    def each(self,items,body):
        n=self.length(items);i=self.alloc();a=self.alloc('address');self.emit('SET',i,'#0');loop=self.label();end=self.label();self.mark(loop)
        self.emit('JUMPIF',self.compare(i,n,'GREATER_EQUAL'),end);self.emit('AT',a,items,i)
        with self.scope():body(a)
        self.emit('ALU',i,i,'#1','ADD');self.emit('JUMP',loop);self.mark(end)
    def collection(self,x):
        if isinstance(x,Ref):
            if x.kind=='collection':return x
            c=self.empty();self.emit('APPEND',c,self.address(x));return c
        if isinstance(x,Call):
            if x.name=='QUERY':return self.query(x.args)
            if x.name in ('REF','R'):return self.collection(self.vars.get(x.args[0],x.args[0]))
            if x.name=='COUNT':
                name='selection_candidates_L'+str(self.line)
                if name not in self.inputs:
                    c=self.alloc('collection',True);self.inputs[name]=c;self.vars[name]=c;self.warning('Source selector supplies a count but no candidate set; declared collection input: '+name)
                return self.inputs[name]
        if isinstance(x,list):
            c=self.empty()
            for part in x:
                src=self.collection(part);self.each(src,lambda a:self.emit('APPEND',c,a))
            return c
        if isinstance(x,dict) and 'all'in x:return self.collection(x['all'])
        if isinstance(x,dict) and 'set_minus'in x:
            items=self.collection(x['set_minus'][0]);excluded=self.collection(x['set_minus'][1]);c=self.empty()
            def retain(a):
                found=self.contains(excluded,a);skip=self.label();self.emit('JUMPIF',found,skip);self.emit('APPEND',c,a);self.mark(skip)
            self.each(items,retain);return c
        if x=='event.targets':
            container=self.load(Ref('A1','address'),'TARGETS','address');items=self.alloc('collection');self.emit('ENUMERATE',items,container);return items
        return self.collection(self.address(x))
    def contains(self,items,object):
        found=self.alloc('flag');self.emit('SET',found,'#0')
        def match(a):
            skip=self.label();self.emit('JUMPIF',self.compare(a,object),skip,'FALSE');self.emit('SET',found,'#1');self.mark(skip)
        self.each(items,match);return found
    def any_equal(self,lhs,values,domain=None):
        result=self.alloc('flag');self.emit('SET',result,'#0');done=self.label()
        for value in values:
            self.emit('COMPARE',result,lhs,self.value(value,domain),'EQUAL');self.emit('JUMPIF',result,done)
        self.mark(done);return result
    def query(self,q):
        unknown=set(q)-{'controller','zones','names','face','count','tags','card_type','exclude','attribute','race','level','atk','def','order','owner','can'}
        if unknown:raise ValueError('unknown query fields '+str(unknown))
        out=self.empty();controller=q.get('controller','ANY');players=[Ref('#0'),Ref('#1')] if controller in ('ANY','BOTH') else [self.player(controller)]
        for player in players:
            zones=q.get('zones',None)
            for zone in zones or [None]:
                container=self.zone_address(player,zone) if zone else self.player_address(player)
                src=self.alloc('collection');self.emit('ENUMERATE',src,container)
                def accept(obj):
                    skip=self.label()
                    def require(flag):self.emit('JUMPIF',flag,skip,'FALSE')
                    if 'owner'in q:require(self.compare(self.load(obj,'OWNER'),self.player(q['owner'])))
                    for key,domain in [('names','NAME'),('card_type','CARD_TYPE'),('attribute','ATTRIBUTE'),('race','RACE')]:
                        if key not in q:continue
                        if key=='card_type':
                            flags=[]
                            for typ in q[key]:
                                if isinstance(typ,str):flags.append(self.compare(self.load(obj,'IS.'+typ),Ref('#1')))
                                else:flags.append(self.compare(self.load(obj,'CARD_TYPE'),self.value(typ,domain)))
                            require(self.combine(flags,False))
                        else:require(self.any_equal(self.load(obj,domain),q[key],domain))
                    if 'face'in q:require(self.compare(self.load(obj,'FACE_UP'),Ref('#1' if q['face']=='FACE_UP' else '#0')))
                    for tag in q.get('tags',[]):
                        if isinstance(tag,dict):name=tag['not'];wanted='#0'
                        else:name=tag;wanted='#1'
                        if name=='EQUIPPED_TO_SELF':require(self.compare(self.load(obj,'EQUIP_TARGET'),Ref('A0','address')))
                        else:require(self.compare(self.load(obj,'TAG.'+name),Ref(wanted)))
                    for capability in q.get('can',[]):require(self.compare(self.load(obj,'CAN.'+capability),Ref('#1')))
                    for excluded in q.get('exclude',[]):require(self.compare(obj,self.address(excluded),'NOT_EQUAL'))
                    for key in ('level','atk','def'):
                        if key not in q:continue
                        for method,rhs in q[key].items():
                            parts=method.split('_');comparison={'lt':'LESS','lte':'LESS_EQUAL','gt':'GREATER','gte':'GREATER_EQUAL','eq':'EQUAL'}[parts[0]]
                            value=self.value(rhs) if len(parts)==1 or parts[1]=='prop' else self.value(Call('R',[rhs]))
                            require(self.compare(self.load(obj,key.upper()),value,comparison))
                    self.emit('APPEND',out,obj);self.mark(skip)
                self.each(src,accept)
        order=q.get('order')
        if order:
            # Ordered source snapshots use top-first order. BOTTOM takes the last match.
            if order not in ('TOP','BOTTOM_MOST_MONSTER'):raise ValueError('unknown query order '+order)
            limited=self.empty();size=self.length(out);done=self.label();self.emit('JUMPIF',self.compare(size,Ref('#0')),done)
            count=q.get('count',{}).get('max','1');count=self.value(count) if count!='-1' else size
            count=self.arithmetic(count,size,'MINIMUM')
            if order=='BOTTOM_MOST_MONSTER':
                a=self.alloc('address');i=self.arithmetic(size,Ref('#1'),'SUBTRACT');self.emit('AT',a,out,i);self.emit('APPEND',limited,a)
            else:
                i=self.alloc();a=self.alloc('address');self.emit('SET',i,'#0');loop=self.label();self.mark(loop);self.emit('JUMPIF',self.compare(i,count,'GREATER_EQUAL'),done);self.emit('AT',a,out,i);self.emit('APPEND',limited,a);self.emit('ALU',i,i,'#1','ADD');self.emit('JUMP',loop)
            self.mark(done);out=limited
        return out
    def combine(self,flags,all_):
        result=self.alloc();self.emit('SET',result,'#1' if all_ else '#0')
        for f in flags:self.emit('ALU',result,result,f,'MULTIPLY' if all_ else 'ADD')
        return self.compare(result,Ref('#0'),'GREATER')
    def predicate(self,p):
        if isinstance(p,Call) and p.name=='EVENT':
            return self.compare(self.load(Ref('A1','address'),str(p.args[0])),Ref('#1'))
        name=p['pred'];obj=p.get('subject','self')
        if name=='COMPARE':return self.compare(self.value(p['lhs']),self.value(p['rhs']),{'==':'EQUAL','=':'EQUAL','!=':'NOT_EQUAL','<':'LESS','<=':'LESS_EQUAL','>':'GREATER','>=':'GREATER_EQUAL'}[p['op']])
        if name in ('ALL','ANY'):return self.combine([self.predicate(c) for c in p['conditions']],name=='ALL')
        if name=='NOT':return self.compare(self.predicate(p['condition']),Ref('#0'))
        if name in ('EXISTS','NOT_EXISTS'):
            c=self.collection(p['selector']);minimum='1'
            if isinstance(p['selector'],Call) and p['selector'].name=='QUERY':minimum=p['selector'].args.get('count',{}).get('min','1')
            minimum=self.arithmetic(self.value(minimum),Ref('#1'),'MAXIMUM');f=self.compare(self.length(c),minimum,'GREATER_EQUAL')
            return f if name=='EXISTS' else self.compare(f,Ref('#0'))
        if name=='IN_SET':return self.any_equal(self.value(p['value']),p['set'])
        if name in ('IN_ZONE','SOURCE_ZONE'):return self.compare(self.load(self.address(obj),'ZONE' if name=='IN_ZONE' else 'SOURCE_ZONE'),Ref('#'+str(self.schema.zone(p['zone']))))
        if name=='FACE_UP':return self.compare(self.load(self.address(obj),'FACE_UP'),Ref('#1'))
        if name in ('RELATION','EVENT_RELATION'):
            field='EQUIP_TARGET' if p['relation']=='EQUIPPED_TO' else 'PREVIOUS_EQUIP_TARGET' if p['relation']=='WAS_EQUIPPED' else 'LINK.'+p['relation']
            return self.compare(self.load(self.address(p['a']),field),self.address(p['b']))
        if name in ('CARD_TYPE','CARD_TYPE_EQUALS'):
            val=p['value']
            if isinstance(val,str):return self.compare(self.load(self.address(obj),'IS.'+val),Ref('#1'))
            return self.compare(self.load(self.address(obj),'CARD_TYPE'),self.value(val,'CARD_TYPE'))
        if name in ('NAME_EQUALS','NAME_IN','EQUIP_TARGET_NAME'):
            if name=='EQUIP_TARGET_NAME':obj='equipped_monster'
            values=p['names'] if name=='NAME_IN' else [p.get('value',p.get('name'))]
            return self.any_equal(self.load(self.address(obj),'NAME'),values,'NAME')
        if name=='TAG':return self.compare(self.load(self.address(obj),'TAG.'+p['value']),Ref('#1'))
        if name=='CONTROLLER':return self.compare(self.load(self.address(obj),'CONTROLLER'),self.player(p['player']))
        if name=='TURN_PLAYER':return self.compare(self.load(Ref('[0]','address'),'TURN_PLAYER'),self.player(p['player']))
        if name=='PHASE':return self.combine([self.compare(self.load(Ref('[0]','address'),'PHASE'),self.value(p['phase'],'PHASE')),self.compare(self.load(Ref('[0]','address'),'TURN_PLAYER'),self.player(p.get('player','PLAYER')))],True)
        if name in ('POSITION','BATTLE_OPPONENT_DEFENSE'):
            if name=='BATTLE_OPPONENT_DEFENSE':obj='event.opponent_monster';pos='DEFENSE'
            else:pos=p['value']
            if pos in ('ATTACK','DEFENSE'):return self.compare(self.load(self.address(obj),'DEFENSE_POSITION'),Ref('#1' if pos=='DEFENSE' else '#0'))
            return self.compare(self.load(self.address(obj),'POSITION'),self.value(pos,'POSITION'))
        if name=='STILL_ON_FIELD':return self.any_equal(self.load(self.address(obj),'ZONE'),[str(self.schema.zone(z)) for z in ('MONSTER','SPELL_TRAP','FIELD')])
        if name in ('ACTIVATION_TIMING_LEGAL','CAN_NORMAL_SUMMON_OR_SET','CAN_SPECIAL_SUMMON'):return self.compare(self.load(self.address(obj),'CAN.'+name.removeprefix('CAN.')),Ref('#1'))
        if name=='LAST_ACTION_SUCCEEDED':return self.compare(Ref('CONTROL.RESULT_SUCCESS'),Ref('#1'))
        if name=='ALL_NAMED_CARDS_EXIST':
            return self.combine([self.predicate({'pred':'EXISTS','selector':Call('QUERY',{'controller':p['controller'],'zones':[p['zone']],'names':[n]})}) for n in p['names']],True)
        if name=='HAND_CONTAINS_NO_OPPONENT_OWNED_CARD':return self.predicate({'pred':'NOT_EXISTS','selector':Call('QUERY',{'controller':'PLAYER','zones':['HAND'],'owner':'OPPONENT'})})
        if name=='ANY_TARGET_FACE_DOWN':
            found=self.alloc();self.emit('SET',found,'#0')
            self.each(self.collection('event.targets'),lambda a:self.emit('ALU',found,found,self.compare(self.load(a,'FACE_UP'),Ref('#0')),'ADD'))
            return self.compare(found,Ref('#0'),'GREATER')
        if name=='EXACTLY_ONE_MONSTER_REMAINS_ALONE_ON_FIELD':return self.compare(self.length(self.query({'controller':'ANY','zones':['MONSTER']})),Ref('#1'))
        if name=='NO_OTHER_CARD_OR_EFFECT_ACTIVATED_THIS_TURN':return self.compare(self.load(self.player_address(self.player(p.get('player','PLAYER'))),'OTHER_ACTIVATIONS_THIS_TURN'),Ref('#0'))
        raise ValueError('unknown predicate '+name)
    def history_count(self,event,selector=None):
        name=event.args[0] if isinstance(event,Call) else event
        out=self.alloc();n=self.alloc();i=self.alloc();kind=self.alloc();self.emit('SET',out,'#0');self.emit('SET',i,'#0');self.emit('HISTORY',n,'COUNT');loop=self.label();done=self.label();skip=self.label();self.mark(loop);self.emit('JUMPIF',self.compare(i,n,'GREATER_EQUAL'),done);self.emit('HISTORY',kind,'KIND',i);self.emit('JUMPIF',self.compare(kind,Ref(self.schema.event(name))),skip,'FALSE')
        if selector:
            a=self.alloc('address');self.emit('HISTORY',a,'SUBJECT',i);items=self.collection(selector);self.emit('JUMPIF',self.contains(items,a),skip,'FALSE')
        self.emit('ALU',out,out,'#1','ADD');self.mark(skip);self.emit('ALU',i,i,'#1','ADD');self.emit('JUMP',loop);self.mark(done);return out
    def remove_from(self,items,object):
        out=self.empty()
        def retain(a):
            skip=self.label();self.emit('JUMPIF',self.compare(a,object),skip);self.emit('APPEND',out,a);self.mark(skip)
        self.each(items,retain);return out
    def selection(self,selector,player='PLAYER',all_=False,random=False,count=None):
        items=self.collection(selector)
        if all_:return items
        limits=selector.args.get('count',{}) if isinstance(selector,Call) and selector.name=='QUERY' else selector.args[0] if isinstance(selector,Call) and selector.name=='COUNT' else {}
        low=self.value(limits.get('min','1'));maximum=limits.get('max','1');available=self.length(items)
        self.guard(self.compare(available,low,'GREATER_EQUAL'))
        if count is not None:amount=self.arithmetic(self.value(count),available,'MINIMUM')
        elif maximum=='1' and limits.get('min','1')=='1':amount=Ref('#1')
        else:
            upper=available if maximum=='-1' else self.arithmetic(self.value(maximum),available,'MINIMUM')
            width=self.arithmetic(self.arithmetic(upper,low,'SUBTRACT'),Ref('#1'),'ADD');amount=self.alloc();self.emit('CHOOSE',amount,width,self.player(player));self.emit('ALU',amount,amount,low,'ADD')
        if amount.operand=='#1':
            chosen=self.alloc('address')
            if random:
                index=self.alloc();self.emit('RANDOM','RANDOM_CARD',index,'NONE',available);self.emit('AT',chosen,items,index)
            else:self.emit('CHOOSE',chosen,items,self.player(player))
            return chosen
        out=self.empty();remaining=self.alloc();self.emit('SET',remaining,amount);loop=self.label();done=self.label();self.mark(loop);self.emit('JUMPIF',self.compare(remaining,Ref('#0'),'LESS_EQUAL'),done)
        chosen=self.alloc('address')
        if random:
            index=self.alloc();self.emit('RANDOM','RANDOM_CARD',index,'NONE',self.length(items));self.emit('AT',chosen,items,index)
        else:self.emit('CHOOSE',chosen,items,self.player(player))
        self.emit('APPEND',out,chosen);rest=self.remove_from(items,chosen);self.emit('SET',items,rest);self.emit('ALU',remaining,remaining,'#1','SUBTRACT');self.emit('JUMP',loop);self.mark(done);return out
    def native_event(self,name,subject='NONE',object='NONE',context='NONE'):
        self.emit('EVENT',name,subject,object,context)
    def phase_condition(self,phase,player=None):
        tests=[self.compare(self.load(Ref('[0]','address'),'PHASE'),self.value(phase,'PHASE'))]
        if player is not None:tests.append(self.compare(self.load(Ref('[0]','address'),'TURN_PLAYER'),self.player(player)))
        return self.combine(tests,True)
    def subscribe(self,event,body,condition=None,once=True,refresh_event=False):
        handle=self.alloc();entry=self.label();after=self.label();end=self.label()
        self.emit('SUBSCRIBE',handle,entry,self.schema.event(event));self.emit('JUMP',after);self.mark(entry)
        if condition:self.emit('JUMPIF',condition(),end,'FALSE')
        if once:self.emit('CANCEL',handle)
        if refresh_event:self.emit('LOAD','A1','CONTROL.EVENT_CONTEXT')
        with self.scope():body()
        self.mark(end);self.emit('HALT');self.mark(after);return handle
    def timing(self,name,body):
        timings={
            'BATTLE_PHASE_END':('PHASE_END','BATTLE',None,1),
            'UNTIL_BATTLE_PHASE_END':('PHASE_END','BATTLE',None,1),
            'BATTLE_PHASE':('PHASE_END','BATTLE',None,1),
            'END_PHASE_THIS_TURN':('PHASE_END','END',None,1),
            'UNTIL_END_PHASE':('PHASE_END','END',None,1),
            'THIS_TURN':('TURN_END',None,None,1),
            'NEXT_CONTROLLER_STANDBY_PHASE':('PHASE_BEGIN','STANDBY','PLAYER',1),
            'NEXT_PLAYER_STANDBY_PHASE':('PHASE_BEGIN','STANDBY','PLAYER',1),
            'SECOND_PLAYER_STANDBY_PHASE_AFTER_ACTIVATION':('PHASE_BEGIN','STANDBY','PLAYER',2),
            'OPPONENT_THIRD_END_PHASE_AFTER_ACTIVATION':('PHASE_END','END','OPPONENT',3),
            'NEXT_DRAW_PHASE_BEFORE_DRAW':('PHASE_BEGIN','DRAW',None,1),
            'DAMAGE_CALCULATION':('DAMAGE_CALCULATION_END',None,None,1),
            'DAMAGE_STEP':('DAMAGE_STEP_END',None,None,1),
        }
        if name in ('UNTIL_END_PHASE_OF_CONTROLLER_NEXT_TURN','UNTIL_END_OF_OPPONENT_NEXT_TURN'):
            player='PLAYER' if name=='UNTIL_END_PHASE_OF_CONTROLLER_NEXT_TURN' else 'OPPONENT'
            now=self.load(Ref('[0]','address'),'TURN')
            condition=lambda:self.combine([self.phase_condition('END',player),self.compare(self.load(Ref('[0]','address'),'TURN'),now,'GREATER')],True)
            return self.subscribe('PHASE_END',body,condition)
        if name not in timings:raise ValueError('unknown timing '+name)
        event,phase,player,repeats=timings[name]
        condition=(lambda:self.phase_condition(phase,player)) if phase else None
        def next_body(n):
            if n==1:body()
            else:self.subscribe(event,lambda:next_body(n-1),condition)
        return self.subscribe(event,lambda:next_body(repeats),condition)
    def expire(self,duration,body,subject=None):
        if duration in (None,'PERMANENT','PERMANENT_FOR_THAT_INSTANCE','REST_OF_DUEL'):return
        if duration=='CONTINUOUS':return # explicit re-evaluation cleanup is emitted in the entry prelude
        if duration in ('WHILE_THIS_FACE_UP','WHILE_EQUIPPED','WHILE_CASTLE_PRESENT'):
            if duration=='WHILE_THIS_FACE_UP':p={'pred':'FACE_UP','subject':'self'}
            elif duration=='WHILE_EQUIPPED':p={'pred':'RELATION','relation':'EQUIPPED_TO','a':'self','b':subject or 'equipped_monster'}
            else:p={'pred':'EXISTS','selector':Call('QUERY',{'controller':'ANY','zones':['SPELL_TRAP','FIELD'],'names':['Castle of Dark Illusions']})}
            return self.subscribe('STATE_CHANGED',body,lambda:self.compare(self.predicate(p),Ref('#0')))
        return self.timing(duration,body)
    def modify_one(self,obj,field,value,method,duration):
        prop=self.field(obj,field);handle=self.alloc(permanent=duration=='THIS_EFFECT');self.emit('MODIFY',handle,prop,value,method)
        if duration=='THIS_EFFECT':
            self.ephemeral.append(handle);return handle
        if duration=='CONTINUOUS':
            key=f'INTERNAL.MODIFIER.{self.effect.card}.{self.effect.number}.{self.line}.{field}'
            self.store(obj,key,handle)
            if key not in self.cleanups:self.cleanups.append(key)
        else:self.expire(duration,lambda:self.emit('UNMODIFY',handle),obj)
        return handle
    def position(self,obj,position):
        if isinstance(position,Call):self.store(obj,'POSITION',self.value(position));return
        if position=='CHOOSE_FACE_UP':
            choice=self.alloc();self.emit('CHOOSE',choice,'#2','V0');position_value=self.arithmetic(choice,self.value('FACE_UP_ATTACK','POSITION'),'ADD');self.store(obj,'POSITION',position_value);self.store(obj,'FACE_UP',Ref('#1'));self.store(obj,'DEFENSE_POSITION',choice);return
        if position=='TOGGLE_BATTLE_POSITION':self.store(obj,'DEFENSE_POSITION',self.arithmetic(Ref('#1'),self.load(obj,'DEFENSE_POSITION'),'SUBTRACT'));return
        if position in ('FACE_UP','FACE_DOWN','FACE_UP_IN_DECK'):self.store(obj,'FACE_UP',Ref('#0' if position=='FACE_DOWN' else '#1'));return
        if position in ('ATTACK','DEFENSE'):self.store(obj,'DEFENSE_POSITION',Ref('#1' if position=='DEFENSE' else '#0'));return
        if position not in ('FACE_UP_ATTACK','FACE_UP_DEFENSE','FACE_DOWN_DEFENSE'):raise ValueError('unknown position '+str(position))
        self.store(obj,'POSITION',self.value(position,'POSITION'));self.store(obj,'FACE_UP',Ref('#0' if position.startswith('FACE_DOWN') else '#1'));self.store(obj,'DEFENSE_POSITION',Ref('#1' if position.endswith('DEFENSE') else '#0'))
    def move(self,p):
        method=p['method'];native={'RETURN_TO_HAND':'RETURN','RETURN_TO_DECK':'RETURN','PLACE_FACE_UP':'RELOCATE'}.get(method,method)
        if native not in ('RELOCATE','DRAW','SEARCH','MILL','DISCARD','TRIBUTE','DESTROY','SEND','BANISH','RETURN','ATTACH_MATERIAL','DETACH_MATERIAL'):raise ValueError('unknown move method '+native)
        total=self.alloc();self.emit('SET',total,'#0');subject=self.collection(p['subject'])
        def relocate(obj):
            destination=p['to'];zone={'GY_OWNERS':'GY','DECK_TOP':'DECK','DECK_BOTTOM':'DECK'}.get(destination,destination)
            owner=self.player(p['player']) if 'player'in p else self.load(obj,'OWNER') if zone in ('GY','HAND','DECK','BANISHED') else Ref('V0')
            dst=self.zone_address(owner,zone)
            flags=4 if self.cost or p.get('cost')=='1' else 0
            cause={'RULE':1,'CARD_EFFECT':3,'REPLACEMENT':7}.get(p.get('cause'),3)
            self.emit('MOVE',native,dst,obj,'#1',flags=flags,cause=cause)
            self.emit('ALU',total,total,'CONTROL.RESULT_COUNT','ADD')
            if destination in ('DECK_TOP','DECK_BOTTOM'):
                order=Ref('#0') if destination=='DECK_TOP' else self.arithmetic(self.load(self.player_address(owner),'DECK_SIZE'),Ref('#1'),'SUBTRACT')
                self.store(obj,'ORDER_INDEX',order)
            if method=='PLACE_FACE_UP':self.position(obj,'FACE_UP')
            if 'position'in p:self.position(obj,p['position'])
        self.each(subject,relocate);self.emit('STORE','CONTROL.RESULT_COUNT',total);self.emit('STORE','CONTROL.RESULT_SUCCESS',self.compare(total,Ref('#0'),'GREATER'))
    def summon(self,p):
        player=self.player(p.get('player','PLAYER'));dest=self.load(self.player_address(player),'FREE_MONSTER_SLOT','address')
        mode=p.get('position','FACE_UP_ATTACK');source=p['subject']
        if isinstance(source,dict) and 'token'in source:
            token=source['token'];prototype=self.value(token.get('name','Mirage Token'),'NAME')
            self.emit('SUMMON','TOKEN','DEFAULT',dest,prototype)
            created=self.load(self.player_address(player),'LAST_SUMMONED','address')
            for key,v in token.items():
                if key in ('copy_fields_from','fields'):continue
                self.store(created,key.upper(),self.value(v,{'name':'NAME','race':'RACE','attribute':'ATTRIBUTE'}.get(key)))
            if 'copy_fields_from'in token:
                origin=self.address(token['copy_fields_from'])
                for field in token['fields']:self.store(created,field,self.load(origin,field))
            self.position(created,mode);self.bind('created_token',created);return
        def one(obj):
            self.emit('SUMMON',p['method'],'DEFAULT',self.load(self.player_address(player),'FREE_MONSTER_SLOT','address'),obj);self.position(obj,mode)
        if 'from_zone'in p:self.guard(self.predicate({'pred':'IN_ZONE','subject':source,'zone':p['from_zone']}))
        self.each(self.collection(source),one)
    def shuffle(self,items):
        size=self.length(items);index=self.alloc();self.emit('SET',index,size);loop=self.label();done=self.label();self.mark(loop);self.emit('JUMPIF',self.compare(index,Ref('#1'),'LESS_EQUAL'),done)
        j=self.alloc();self.emit('RANDOM','SHUFFLE_SWAP',j,'NONE',index);self.emit('ALU',index,index,'#1','SUBTRACT');a=self.alloc('address');b=self.alloc('address');self.emit('AT',a,items,index);self.emit('AT',b,items,j)
        va=self.load(a,'ORDER_INDEX');vb=self.load(b,'ORDER_INDEX');self.store(a,'ORDER_INDEX',vb);self.store(b,'ORDER_INDEX',va);self.emit('JUMP',loop);self.mark(done)
        self.native_event('SHUFFLE')
    def actions(self,actions):
        for item in actions:
            args=dict(item);op=args.pop('op');self.node(Node(op,args,self.line))
    def node(self,n):
        old_line=self.line;self.line=n.line;self.source_lines.add(n.line)
        try:
            with self.scope():self.statement(n)
        except Exception as e:
            raise ValueError(f'{self.effect.card}/{self.effect.number} line {n.line} {n.op}: {e}') from e
        finally:self.line=old_line
    def nodes(self,nodes):
        for n in nodes:self.node(n)
    def statement(self,n):
        op,p=n.op,n.args
        if op=='FLAGS':return
        if op=='EFFECT_END:':return
        if op=='HALT.EFFECT':self.emit('JUMP',self.exit);return
        if op=='NOP':self.emit('NOP',meaning='Explicit NOP in the source; resolution is unspecified.');self.warning('Source explicitly contains no resolution instruction.');return
        if op=='TEST':self.last=self.predicate(p);return
        if op=='JIF_FALSE':
            if self.last is None:raise ValueError('branch without TEST')
            self.emit('JUMPIF',self.last,self.exit,'FALSE');return
        if op in ('EVENT.MATCH','REQUIRE_EVENT'):
            event=p.args[0] if isinstance(p,Call) else p['event'].args[0];self.guard(self.compare(Ref('CONTROL.EVENT_KIND'),Ref(self.schema.event(event))));return
        if op=='REQUIRE':self.guard(self.predicate(p['condition']));return
        if op in ('CMP/JIF','IF'):
            flag=self.predicate(p if op=='CMP/JIF' else p['condition']);other=self.label();end=self.label();self.emit('JUMPIF',flag,other,'FALSE')
            if op=='CMP/JIF':self.nodes(n.children)
            else:self.actions(p['then'])
            self.emit('JUMP',end);self.mark(other)
            if op=='CMP/JIF':self.nodes(n.otherwise or [])
            else:self.actions(p.get('else',[]))
            self.mark(end);return
        if op=='COST:':
            old=self.cost;self.cost=True
            self.nodes(n.children);self.cost=old;return
        if op=='CHOICE':
            choice=self.alloc();labels=[label for label,_ in n.children]
            self.emit('CHOOSE',choice,'#'+str(len(labels)),self.player(p['player']));end=self.label()
            for index,(label,body) in enumerate(n.children):
                skip=self.label();self.emit('JUMPIF',self.compare(choice,Ref('#'+str(index))),skip,'FALSE');self.nodes(body);self.emit('JUMP',end);self.mark(skip)
            self.mark(end);return
        if op=='OPTIONAL':
            choice=self.alloc();self.emit('CHOOSE',choice,'#2',self.player(p['player']));end=self.label();self.emit('JUMPIF',self.compare(choice,Ref('#0')),end);self.actions(p['actions']);self.mark(end);return
        if op=='SCHEDULE':self.timing(p['timing'],lambda:self.nodes(n.children));return
        if op=='APPLY_TRIGGER':
            handle=self.subscribe(p['when'].args[0],lambda:self.actions(p['actions']),once=False,refresh_event=True);self.expire(p['duration'],lambda:self.emit('CANCEL',handle));return
        if op=='SCHEDULE_CONDITION':
            if p['window']!='PLAYER_NEXT_TURN':raise ValueError('unknown time window')
            current=self.load(Ref('[0]','address'),'TURN');offset=self.arithmetic(Ref('#1'),self.compare(self.load(Ref('[0]','address'),'TURN_PLAYER'),Ref('V0')),'ADD');target=self.arithmetic(current,offset,'ADD')
            handle=self.subscribe(p['condition'].args[0],lambda:self.actions(p['actions']),condition=lambda:self.compare(self.load(Ref('[0]','address'),'TURN'),target),once=False,refresh_event=True)
            self.subscribe('TURN_END',lambda:self.emit('CANCEL',handle),condition=lambda:self.compare(self.load(Ref('[0]','address'),'TURN'),target,'GREATER_EQUAL'));return
        if op in ('SELECT','SELECT.TARGET','TARGET','OPPONENT_SELECT','SELECT_ALL','SELECT_AND_MOVE'):
            who='OPPONENT' if op=='OPPONENT_SELECT' else p.get('player','PLAYER')
            selected=self.selection(p['selector'],who,all_=op=='SELECT_ALL')
            if op=='SELECT_AND_MOVE':self.move({'method':p['selection'],'subject':selected,'to':p['to'],'cost':p.get('cost','0')})
            else:
                selected=self.bind(p['name'],selected)
                if op in ('SELECT.TARGET','TARGET') or p.get('selection')=='TARGET':self.each(self.collection(selected),lambda a:self.native_event('TARGET',a,self.player_address(self.player(who))))
            return
        if op=='RANDOM':
            kind=p['kind']
            if kind in ('DIE','COIN'):
                out=self.alloc();self.emit('RANDOM',kind,out,'NONE',self.value(p.get('sides','2')))
                if kind=='DIE':self.emit('ALU',out,out,'#1','ADD')
                else:self.emit('ALU',out,out,self.value('HEADS','COIN'),'ADD')
            elif kind in ('CARD','CARD_SET'):
                out=self.selection(p['from'],p.get('player','PLAYER'),random=True,count=p.get('count') if kind=='CARD_SET' else None)
            else:raise ValueError('unknown random kind '+kind)
            self.bind(p['store'],out);return
        if op=='MOVE':self.move(p);return
        if op=='SUMMON':self.summon(p);return
        if op=='POSITION':self.each(self.collection(p['subject']),lambda obj:self.position(obj,p['position']));return
        if op in ('EQUIP','UNEQUIP'):
            equipment=self.address(p['equipment'])
            if op=='EQUIP':
                target=self.address(p['target']);self.store(equipment,'EQUIP_TARGET',target);self.native_event('EQUIPPED',equipment,target)
            else:
                target=self.load(equipment,'EQUIP_TARGET','address');self.store(equipment,'EQUIP_TARGET',Ref('#0'));self.native_event('UNEQUIPPED',equipment,target)
            return
        if op in ('MODIFY','SET_EQUAL','SWAP_STATS'):
            def modify(obj):
                if op=='SWAP_STATS':
                    left,right=p['fields'];lv=self.load(obj,left);rv=self.load(obj,right);self.modify_one(obj,left,rv,'SET',p.get('duration'));self.modify_one(obj,right,lv,'SET',p.get('duration'));return
                method=p.get('operation','SET');raw=p.get('value',p.get('source'));duration=p.get('duration','CONTINUOUS' if op=='SET_EQUAL' else None)
                if isinstance(raw,str) and re.fullmatch(r'#-?\d+\.\d+',raw):
                    fraction=Fraction(raw[1:])
                    if method!='MULTIPLY':raise ValueError('fraction requires multiply')
                    if fraction.numerator!=1:self.modify_one(obj,p['field'],Ref('#'+str(fraction.numerator)),'MULTIPLY',duration)
                    self.modify_one(obj,p['field'],Ref('#'+str(fraction.denominator)),'DIVIDE',duration);return
                v=self.value(raw)
                if method=='SUBTRACT':v=self.arithmetic(Ref('#0'),v,'SUBTRACT');method='ADD'
                self.modify_one(obj,p['field'],v,method,duration)
            self.each(self.collection(p['subject']),modify);return
        if op=='RESTRICT':
            def restrict(obj):
                end=self.label()
                for key in ('condition','while_'):
                    if key in p:self.emit('JUMPIF',self.predicate(p[key]),end,'FALSE')
                field='RESTRICTION.'+p['restriction'];handle=self.modify_one(obj,field,Ref('#1'),'SET',p['duration'])
                if 'while_'in p:self.subscribe('STATE_CHANGED',lambda:self.emit('UNMODIFY',handle),condition=lambda:self.compare(self.predicate(p['while_']),Ref('#0')))
                for key in ('subject_ref','target_ref','name'):
                    if key in p:self.store(obj,field+'.'+key.upper(),self.address(p[key]) if key!='name' else self.value(p[key],'NAME'))
                self.mark(end)
            self.each(self.collection(p['subject']),restrict);return
        if op=='CONTROL':
            self.each(self.collection(p['subject']),lambda obj:self.modify_one(obj,'CONTROLLER',self.player(p['player']),'SET',p['duration']));return
        if op=='COUNTER':
            obj=self.address(p['subject']);field='COUNTER.'+p['kind'];amount=self.value(p['amount']);method=p['operation']
            if method=='SET':v=amount
            else:
                old=self.load(obj,field)
                if method=='REMOVE':self.guard(self.compare(old,amount,'GREATER_EQUAL'))
                v=self.arithmetic(old,amount,'ADD' if method=='PLACE' else 'SUBTRACT')
            self.store(obj,field,v);return
        if op in ('DAMAGE','LP'):
            amount=self.value(p.get('amount',p.get('value')));player=self.player_address(self.player(p['player']));target=self.field(player,'LP')
            native='DAMAGE' if op=='DAMAGE' else {'PAY':'PAY_LP','GAIN':'GAIN_LP','ADD':'GAIN_LP'}[p.get('operation',p.get('method'))]
            flags=(4 if native=='PAY_LP' or self.cost or p.get('cost')=='1' else 0) | (64 if 'MANDATORY'in p.get('flags',[]) else 0)
            self.emit(native,target,amount,flags=flags,cause=6 if 'MAINTENANCE'in p.get('flags',[]) else 3);return
        if op in ('DRAW','DRAW_UNTIL_HAND_SIZE'):
            players=[Ref('#0'),Ref('#1')] if p['player']=='BOTH' else [self.player(p['player'])]
            for player in players:
                hand=self.zone_address(player,'HAND');deck=self.zone_address(player,'DECK')
                if op=='DRAW':amount=self.value(p['count'])
                else:
                    have=self.alloc();self.emit('COUNT',have,hand);amount=self.arithmetic(self.arithmetic(self.value(p['size']),have,'SUBTRACT'),Ref('#0'),'MAXIMUM')
                self.emit('MOVE','DRAW',hand,deck,amount)
                if 'store_count'in p:self.bind(p['store_count'],Ref('CONTROL.RESULT_COUNT'))
            return
        if op in ('SNAPSHOT','SNAPSHOT_COUNT','SNAPSHOT_SUM'):
            if op=='SNAPSHOT':v=self.value(p['value'])
            elif op=='SNAPSHOT_COUNT':v=self.length(self.collection(p['selector']))
            else:
                v=self.alloc();self.emit('SET',v,'#0');self.each(self.collection(p['selector']),lambda a:self.emit('ALU',v,v,self.load(a,p['property']),'ADD'))
            self.bind(p['name'],v);return
        if op=='COUNT_HISTORY':self.bind(p['name'],self.history_count(p['event'],p['selector']));return
        if op=='COLLECT':self.bind(p['name'],self.collection(p['subjects']));return
        if op=='PERSIST_BIND':self.store(Ref('A0','address'),'PERSIST.'+p['key'],self.value(p['value']));return
        if op=='PERSIST_CLEAR':
            for key in p['keys']:self.store(Ref('A0','address'),'PERSIST.'+key,Ref('#0'))
            return
        if op=='LINK':self.store(self.address(p['a']),'LINK.'+p['relation'],self.address(p['b']));return
        if op in ('ADD_ALIAS','REMOVE_ALIAS'):self.store(self.address(p['subject']),'ALIAS.'+p['name'],Ref('#1' if op=='ADD_ALIAS' else '#0'));return
        if op=='ALLOW_NORMAL_SET':self.store(self.address(p['subject']),'CAN.NORMAL_SET',Ref('#1'));return
        if op=='DECLARE_RITUAL_COMPATIBILITY':self.store(self.address(p['subject']),'RITUAL_SPELL',self.value(p['ritual_spell'],'NAME'));return
        if op=='FUSION_MATERIALS':
            self.store(Ref('A0','address'),'FUSION_MATERIAL_COUNT',self.number(len(p['materials'])))
            for i,name in enumerate(p['materials']):self.store(Ref('A0','address'),'FUSION_MATERIAL.'+str(i),self.value(name,'NAME'))
            return
        if op=='DECLARE':
            domain=p['domain'];choice=self.alloc()
            if isinstance(domain,list):
                self.emit('CHOOSE',choice,'#'+str(len(domain)),self.player(p['player']));out=self.alloc();end=self.label()
                enum='COIN' if 'HEADS'in domain else 'CARD_TYPE'
                for i,item in enumerate(domain):
                    skip=self.label();self.emit('JUMPIF',self.compare(choice,self.number(i)),skip,'FALSE');self.emit('SET',out,self.value(item,enum));self.emit('JUMP',end);self.mark(skip)
                self.mark(end)
            elif isinstance(domain,dict):
                low=self.value(domain['integer']['min']);high=self.value(domain['integer']['max']);self.emit('CHOOSE',choice,self.arithmetic(self.arithmetic(high,low,'SUBTRACT'),Ref('#1'),'ADD'),self.player(p['player']));out=self.arithmetic(choice,low,'ADD')
            elif domain=='CARD_NAME':
                self.emit('CHOOSE',choice,self.load(Ref('[0]','address'),'CARD_NAME_COUNT'),self.player(p['player']));out=self.arithmetic(choice,Ref('#1'),'ADD')
            else:raise ValueError('unknown declaration domain')
            self.bind(p['store'],out);return
        if op=='SEARCH_SOURCE':
            items=self.query({'controller':'PLAYER','zones':p['zones'],'names':[p['names']]});self.bind(p['store'],self.address(items));return
        if op=='DETERMINE_NEXT_NAME':
            items=self.collection(p['based_on']);out=self.alloc();self.emit('SET',out,'#0');end=self.label()
            for name in p['sequence']:
                found=self.alloc();self.emit('SET',found,'#0')
                self.each(items,lambda a:self.emit('ALU',found,found,self.compare(self.load(a,'NAME'),self.value(name,'NAME')),'ADD'))
                skip=self.label();self.emit('JUMPIF',self.compare(found,Ref('#0'),'GREATER'),skip);self.emit('SET',out,self.value(name,'NAME'));self.emit('JUMP',end);self.mark(skip)
            self.mark(end);self.bind(p['store'],out);return
        if op=='FIND':self.bind(p['name'],self.address(self.collection(p['selector'])));return
        if op in ('EXCAVATE','EXCAVATE_UNTIL'):
            found=self.alloc('flag');self.emit('SET',found,'#0')
            player=self.player(p['player']);items=self.alloc('collection');self.emit('ENUMERATE',items,self.zone_address(player,p['zone']));out=self.empty();match=self.alloc('address');n=self.length(items);i=self.alloc();self.emit('SET',i,'#0');loop=self.label();done=self.label();self.mark(loop);self.emit('JUMPIF',self.compare(i,n,'GREATER_EQUAL'),done);self.emit('AT',match,items,i);self.emit('APPEND',out,match);self.native_event('EXCAVATE',match,self.player_address(player));self.emit('ALU',i,i,'#1','ADD')
            if op=='EXCAVATE_UNTIL':
                self.vars['excavated_card']=match;self.emit('SET',found,self.predicate(p['predicate']));self.emit('JUMPIF',found,done);del self.vars['excavated_card']
            else:self.emit('JUMPIF',self.compare(i,self.value(p['count']),'GREATER_EQUAL'),done)
            self.emit('JUMP',loop);self.mark(done)
            if op=='EXCAVATE':self.bind(p['store'],self.address(out))
            else:
                matched=self.label();self.emit('JUMPIF',found,matched)
                self.move({'method':'SEND','subject':out,'to':'GY'});self.emit('JUMP',self.exit)
                self.mark(matched);self.bind(p['store_cards'],out);self.bind(p['store_match'],match)
                self.warning('Source omits the no-match excavation branch; exhausted candidates are sent to GY and resolution stops.')
            return
        if op in ('INSPECT','REVEAL'):
            who=self.player_address(self.player(p.get('player','PLAYER')))
            flags=16 if p.get('public')=='1' else 32 if p.get('private')=='1' else 0
            self.each(self.collection(p['subject']),lambda a:self.emit('EVENT','INSPECT' if op=='INSPECT' else 'REVEAL',a,who,'#1' if p.get('public')=='1' else '#0',flags=flags));return
        if op=='RETURN_SAME_POSITION':
            # INSPECT is observational; explicitly preserve each object's order.
            self.each(self.collection(p['subject']),lambda a:self.store(a,'ORDER_INDEX',self.load(a,'ORDER_INDEX')));return
        if op=='SHUFFLE':
            for player in ([Ref('#0'),Ref('#1')] if p['player']=='BOTH' else [self.player(p['player'])]):
                items=self.alloc('collection');self.emit('ENUMERATE',items,self.zone_address(player,p['zone']));self.shuffle(items)
            return
        if op=='SHUFFLE_FIELD_IDENTITIES':self.shuffle(self.collection(p['subjects']));return
        if op=='SHUFFLE_INTO_DECK':
            subjects=self.collection(p['subject']);owners=[]
            for player in (Ref('#0'),Ref('#1')):
                present=self.alloc();self.emit('SET',present,'#0')
                self.each(subjects,lambda obj:self.emit('ALU',present,present,self.compare(self.load(obj,'OWNER'),player),'ADD'))
                owners.append(present)
            self.move({'method':'RETURN_TO_DECK','subject':subjects,'to':'DECK_BOTTOM'})
            for index,present in enumerate(owners):
                skip=self.label();self.emit('JUMPIF',self.compare(present,Ref('#0')),skip)
                self.statement(Node('SHUFFLE',{'player':Ref('#'+str(index)),'zone':'DECK'},n.line));self.mark(skip)
            return
        if op=='TREAT_AS_MONSTER':
            self.each(self.collection(p['subject']),lambda a:[self.modify_one(a,key.upper(),self.value(v,'CARD_TYPE' if key=='type' else None),'SET',p['duration']) for key,v in p['stats'].items()]);return
        if op=='FORCE_ACTIVATION':self.native_event('ACTIVATE',self.address(p['subject']));return
        if op=='FORCE_ATTACK':
            attacker=self.address(p['attacker']);target=self.address(p['target']) if 'target'in p else self.load(Ref('A1','address'),'ATTACK_TARGET','address')
            if 'battle_damage'in p:self.store(Ref('[0]','address'),'BATTLE_DAMAGE_OVERRIDE',self.value(p['battle_damage']))
            if 'target_policy'in p:self.store(Ref('[0]','address'),'ATTACK_TARGET_POLICY',self.value(p['target_policy'],'ATTACK_TARGET_POLICY'))
            self.native_event('ATTACK_DECLARED',attacker,target);return
        if op=='NEGATE':
            if p['what'] in ('EVENT_EFFECT','EVENT_ACTIVATION','EVENT_DAMAGE','ATTACK'):
                self.store(Ref('A1','address'),'NEGATED',Ref('#1'));self.native_event('ATTACK_CANCELED' if p['what']=='ATTACK' else 'ACTIVATION_NEGATED' if p['what']=='EVENT_ACTIVATION' else 'EFFECT_NEGATED');return
            self.each(self.collection(p.get('subject','event.subject')),lambda obj:self.modify_one(obj,'EFFECTS_NEGATED',Ref('#1'),'SET',p.get('duration','THIS_EFFECT')));return
        if op=='PREVENT_DESTRUCTION':self.emit('STORE','CONTROL.PENDING_CANCELLED','#1');return
        if op=='REDIRECT_TARGET':self.emit('STORE','CONTROL.PENDING_SOURCE1',self.address(p['new_target']));return
        if op=='REDO_RANDOM_EVENT':
            container=self.load(Ref('A1','address'),'RANDOM_RESULTS','address');items=self.alloc('collection');self.emit('ENUMERATE',items,container)
            def redo(obj):
                roll=self.alloc();self.emit('RANDOM','COIN',roll,'NONE','#2');self.store(obj,'VALUE',self.arithmetic(roll,self.value('HEADS','COIN'),'ADD'))
            self.each(items,redo);return
        if op=='RESET_EFFECT_STATE':
            self.store(self.address(p['subject']),'EFFECT_STATE.'+p['effect'],Ref('#0'));return
        if op=='SKIP_TURN':self.store(self.player_address(self.player(p['player'])),'SKIP_NEXT_TURN',Ref('#1'));return
        if op in ('WIN','DRAW_GAME'):
            result=Ref('#3') if op=='DRAW_GAME' else self.arithmetic(self.player(p['player']),Ref('#1'),'ADD');self.store(Ref('[0]','address'),'RESULT',result);self.native_event('DRAW' if op=='DRAW_GAME' else 'WIN');return
        raise ValueError('unhandled source operation '+op)
    def finish(self):
        self.nodes(self.effect.nodes);self.mark(self.exit)
        for handle in self.ephemeral:
            skip=self.label();self.emit('JUMPIF',self.compare(handle,Ref('#0')),skip);self.emit('UNMODIFY',handle);self.mark(skip)
        self.emit('HALT')
        if self.cleanups or self.ephemeral:
            # Clear this continuous program's prior contributions before its guards.
            saved_code,saved_labels=self.code,self.labels;self.code=[];self.labels={}
            with self.scope():
                for handle in self.ephemeral:self.emit('SET',handle,'#0')
                objects=self.alloc('collection');self.emit('ENUMERATE',objects,'[0]' if self.cleanups else 'NONE')
                # Player-scoped restrictions also need their previous handles cleared.
                self.emit('APPEND',objects,'[0:0]');self.emit('APPEND',objects,'[1:0]');self.emit('APPEND',objects,'[0]')
                for field in self.cleanups:
                    def clear(obj):
                        handle=self.load(obj,field);skip=self.label();self.emit('JUMPIF',self.compare(handle,Ref('#0')),skip);self.emit('UNMODIFY',handle);self.store(obj,field,Ref('#0'));self.mark(skip)
                    self.each(objects,clear)
            offset=len(self.code);self.labels.update({key:pc+offset for key,pc in saved_labels.items()});self.code.extend(saved_code)
        lines=[]
        for pc,(op,args,line,meaning,flags,cause) in enumerate(self.code):
            args=[str(self.labels[a]) if a in self.labels else a for a in args]
            instruction=op+(' '+', '.join(args) if args else '')
            if flags or cause:instruction+=f' | flags={flags} cause={cause}'
            lines.append(f'{pc:5} {instruction:<76} ; {meaning} [source:{line}]')
        return '\n'.join(lines)
