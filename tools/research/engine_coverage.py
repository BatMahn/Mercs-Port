#!/usr/bin/env python3
"""Inventory source inheritance and conservative decompiler navigation candidates.

This reports byte identity and mechanical matches only. It does not estimate
behavioral parity, certify any address comment, or edit game implementation.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re

TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|(?P<line>//[^\n]*)|(?P<block>/\*[\s\S]*?\*/)')
ADDRESS = re.compile(r'\b0x([0-9a-fA-F]{5,8})\b|\bFUN_([0-9a-fA-F]{8})\b')
FUNCTION_HEADER = re.compile(r'^/\* ---- ([0-9a-fA-F]{8}) (.*?) ---- \*/\s*$', re.M)
GENERATED_IDENTIFIER = re.compile(r'\b(?:_?DAT|FUN|LAB|UNK|PTR(?:_DAT)?)_[0-9a-fA-F]{5,16}\b')

def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def source_files(root: Path) -> dict[str, Path]:
    return {str(p.relative_to(root)): p for d in ('src','tools')
            if (root/d).is_dir() for p in (root/d).rglob('*')
            if p.is_file() and p.suffix in ('.c','.h')}

def old_functions(path: Path) -> list[dict]:
    text = path.read_text(errors='replace')
    headers = list(FUNCTION_HEADER.finditer(text))
    if not headers: raise ValueError('No supplied-decompiler function headers found')
    return [{'address':h.group(1).lower(),'name':h.group(2),
             'text':text[h.end():headers[i+1].start() if i+1<len(headers) else len(text)]}
            for i,h in enumerate(headers)]

def normalized_c(text: str, entry_name: str) -> tuple[str,int]:
    # Preserve literal values/operators and variable names. Only remove comments
    # and replace generated relocated symbol names in first-occurrence order.
    text=TOKEN.sub(lambda m:'' if m.group('line') or m.group('block') else m.group(),text)
    text=re.sub(r'\b'+re.escape(entry_name)+r'\b','ENTRY',text)
    symbols: dict[str,str]={}
    def replace(m):
        value=m.group();family=value.split('_')[0] or 'DAT'
        if value not in symbols:symbols[value]=family+'ROLE'+str(len(symbols))
        return symbols[value]
    text=GENERATED_IDENTIFIER.sub(replace,text)
    text=re.sub(r'\b(s_[A-Za-z0-9_]*?)_[0-9a-fA-F]{8}\b',r'\1',text)
    words=re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|0x[0-9a-fA-F]+|\d+|\S',text)
    return digest(' '.join(words).encode()),len(words)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',required=True,type=Path)
    parser.add_argument('--current',required=True,type=Path)
    parser.add_argument('--mw2-decomp',required=True,type=Path)
    parser.add_argument('--exports',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    old=source_files(args.reference);new=source_files(args.current)
    rows=[]
    for name in sorted(old.keys()|new.keys()):
        a=old[name].read_bytes() if name in old else None
        b=new[name].read_bytes() if name in new else None
        state='new' if a is None else 'removed' if b is None else 'byte_identical' if a==b else 'modified'
        rows.append({'path':name,'status':state,'mw2_sha256':digest(a) if a is not None else None,
                     'current_sha256':digest(b) if b is not None else None,
                     'mw2_lines':len(a.splitlines()) if a is not None else 0,
                     'current_lines':len(b.splitlines()) if b is not None else 0})
    baseline=old_functions(args.mw2_decomp)
    baseline_entries={f['address'] for f in baseline}
    indexes={name:json.loads((args.exports/name/'functions.json').read_text())
             for name in ('retail-dos','retail-windows')}
    entry_sets={name:{f['address'] for f in index} for name,index in indexes.items()}
    comments=[]
    for name,path in sorted(new.items()):
        text=path.read_text(errors='replace')
        for comment in TOKEN.finditer(text):
            if not (comment.group('line') or comment.group('block')):continue
            body=comment.group();mentioned_mercs=bool(re.search(r'\bmerc(?:s|enaries)\b',body,re.I))
            for address in ADDRESS.finditer(body):
                value=int(address.group(1) or address.group(2),16)
                # Keep only values in candidate engine address ranges. This can
                # still include constants rather than addresses; never certify.
                if not (0x10000<=value<=0x9c657 or 0x10001000<=value<=0x100a05ff):continue
                canonical=f'{value:08x}'
                exact=[edition for edition,entries in entry_sets.items() if canonical in entries]
                comments.append({'file':name,'line':text.count('\n',0,comment.start()+address.start())+1,
                    'value':'0x'+canonical,'comment_mentions_mercs':mentioned_mercs,
                    'mw2_dos_reference_entry':canonical in baseline_entries,
                    'mercs_exact_entry_candidates':exact,
                    'classification':'comment_address_candidate_only',
                    'comment_excerpt':' '.join(body.split())[:500]})
    old_groups=defaultdict(list);new_groups=defaultdict(list)
    for f in baseline:
        signature,count=normalized_c(f['text'],f['name'])
        if count>=80:old_groups[signature].append((f,count))
    for f in indexes['retail-dos']:
        if not f.get('decompiled') or not f.get('entry_in_executable_memory'):continue
        p=args.exports/'retail-dos/functions'/(f['address']+'.c')
        signature,count=normalized_c(p.read_text(),f['name'])
        if count>=80:new_groups[signature].append((f,count))
    matches=[]
    for signature in sorted(old_groups.keys()&new_groups.keys()):
        olds=old_groups[signature];news=new_groups[signature]
        if len(olds)!=1 or len(news)!=1:continue
        a,count=olds[0];b,_=news[0]
        matches.append({'mw2_dos_address':'0x'+a['address'],'mw2_name':a['name'],
                        'mercs_dos_address':'0x'+b['address'],'mercs_name':b['name'],
                        'normalized_c_sha256':signature,'tokens':count,
                        'classification':'heuristic_unique_normalized_c_candidate_not_verified',
                        'mercs_c_has_warning':b.get('generated_c_contains_warning',False)})
    output={'generated_utc':datetime.now(timezone.utc).isoformat(),
            'scope':'C/H files recursively under src/ and tools/; byte identity only, not behavioral audit coverage.',
            'reference_root':str(args.reference.resolve()),'current_root':str(args.current.resolve()),
            'mw2_decomp':{'path':str(args.mw2_decomp.resolve()),'sha256':digest(args.mw2_decomp.read_bytes()),'functions':len(baseline)},
            'counts':dict(sorted(Counter(x['status'] for x in rows).items())),
            'by_directory':{d:dict(sorted(Counter(x['status'] for x in rows if x['path'].startswith(d+'/')).items())) for d in ('src','tools')},
            'files':rows,
            'source_comment_address_candidates':comments,
            'comment_candidate_counts':{'total_occurrences':len(comments),'comments_explicitly_mentioning_mercs':sum(x['comment_mentions_mercs'] for x in comments),
                'matching_mw2_dos_function_entry':sum(x['mw2_dos_reference_entry'] for x in comments),
                'matching_any_mercs_function_entry':sum(bool(x['mercs_exact_entry_candidates']) for x in comments)},
            'normalized_c_method':'Comments/whitespace removed; relocated autogenerated symbol identifiers renamed by first-occurrence role; literal values/operators/variable names retained. Unique hashes with >=80 tokens only. This does not establish semantic equivalence or validated correspondence.',
            'normalized_c_candidates':sorted(matches,key=lambda x:x['mw2_dos_address']),
            'decompilation_summaries':{d:json.loads((args.exports/d/'summary.json').read_text()) for d in indexes}}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({'counts':output['counts'],'by_directory':output['by_directory'],
                      'comment_candidate_counts':output['comment_candidate_counts'],
                      'mw2_decompiled_functions':len(baseline),'heuristic_normalized_c_candidates':len(matches)},indent=2))

if __name__=='__main__':main()
