#!/usr/bin/env python3
"""machine-grade: 编译每个 Pxx 的 src + dump harness，提取提交真值，与 truth JSON 逐字段比对。
仅主 agent 使用，作为正确性客观依据。不进 blind-package（truth 也仅在 scorer-reference）。"""
import json, subprocess, sys, os, tempfile, re

ROOT = "/home/steven/app/agent_benchmark/benchmark/20260620-1936-cpp-benchmark-v2"
BP = f"{ROOT}/scoring/blind-package"
REF = f"{ROOT}/scoring/scorer-reference"
HARNESS_SRC = f"{ROOT}/scoring/dump_harness.cpp.txt"

# enum 索引（与 hpp 顺序一致）
IDP = {0:"strict",1:"keep_blank",2:"auto_gen"}
RS  = {0:"none",1:"stage",2:"vehicle",3:"range"}
BR  = {0:"telemetry",1:"command",2:"config",3:"event"}

def load_q1_truth():
    return json.load(open(f"{REF}/q01_truth.json"))
def load_q2_truth():
    return json.load(open(f"{REF}/q02_truth.json"))

def dump_submission(p, qdir, qname):
    """编译 Pxx/qdir 的 src + harness，运行，返回 {name: dict10fields}。失败返回 None。"""
    src = os.path.join(BP, p, qdir, "src")
    inc = os.path.join(BP, p, qdir, "include")
    cpps = sorted([os.path.join(src,f) for f in os.listdir(src) if f.endswith(".cpp")])
    # 探测实际头文件名（q01 range_validator.hpp / q02 protocol_adapter.hpp）
    headers = [f for f in os.listdir(inc) if f.endswith(".hpp")]
    header = headers[0] if headers else "range_validator.hpp"
    with tempfile.TemporaryDirectory() as td:
        harness = os.path.join(td, "harness.cpp")
        txt = open(HARNESS_SRC).read().replace("range_validator.hpp", header)
        open(harness,"w").write(txt)
        binp = os.path.join(td, "dump")
        cmd = ["g++","-std=c++17","-O0","-w","-I",inc]+cpps+[harness,"-o",binp]
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            if r.returncode != 0:
                return {"_compile_error": r.stderr[-1500:]}
            r2 = subprocess.run([binp], capture_output=True, text=True, timeout=30)
            if r2.returncode != 0:
                return {"_run_error": r2.stderr[-800:]}
        except subprocess.TimeoutExpired:
            return {"_timeout": True}
    out = {}
    for line in r2.stdout.strip().splitlines():
        parts = line.split("|")
        name = parts[0]
        if name == "": continue
        # fields: name|rc|sr|idp|rs|stamp|ord|mf|pv|br|bmin|bmax|acc_ns|acc_s|rep|f0f1f2f3f4f5|prank|br2|wlo|whi|wmid
        d = {}
        d["route_code"]=int(parts[1]); d["schema_rev"]=int(parts[2])
        d["id_policy"]=IDP.get(int(parts[3]),"?"); d["replay_scope"]=RS.get(int(parts[4]),"?")
        d["stamp_required"]=parts[5]=="1"; d["ordinal_required"]=parts[6]=="1"
        d["mode_flag"]=int(parts[7]); d["phase_value"]=int(parts[8])
        d["branch"]=BR.get(int(parts[9]),"?")
        d["band"]=[int(parts[10]),int(parts[11])]
        # 派生（用于辅助核对，不计入 10 维真值表，但可单独评）
        d["_acc_nostamp"]=parts[12]; d["_acc_stamp"]=parts[13]; d["_replayable"]=parts[14]
        d["_flags"]=parts[15]; d["_prank"]=parts[16]; d["_branch2"]=parts[17]
        d["_wlo"]=parts[18]; d["_whi"]=parts[19]; d["_wmid"]=parts[20]
        out[name]=d
    return out

def compare_q1(sub, truth):
    """10 维真值对比。truth[name] 含 route_code,schema_rev,id_policy,replay_scope,stamp_required,ordinal_required,mode_flag,phase_value,branch,band[min,max]"""
    total=0; correct=0; errors=[]
    if not isinstance(sub, dict) or "_compile_error" in sub or "_timeout" in sub:
        return 0, 0, [f"NO_VALID_SUBMISSION: {sub}"]
    for name, tv in truth.items():
        sv = sub.get(name)
        if sv is None:
            for k in tv: errors.append(f"{name}.{k}: MISSING"); total+=1
            continue
        for k, tvv in tv.items():
            total+=1
            svv = sv.get(k)
            # band 对比
            if k=="band":
                if list(svv)==list(tvv): correct+=1
                else: errors.append(f"{name}.band: {svv} != {tvv}")
            elif svv==tvv:
                correct+=1
            else:
                errors.append(f"{name}.{k}: {svv} != {tvv}")
    return correct, total, errors

def compare_q2(sub, truth):
    """system_B 真值。10 维: route_code,schema_rev,id_policy,replay_scope,stamp_required,ordinal_required,mode_flag,phase_value,branch,band_min,band_max"""
    total=0; correct=0; errors=[]
    if not isinstance(sub, dict) or "_compile_error" in sub or "_timeout" in sub:
        return 0, 0, [f"NO_VALID_SUBMISSION: {sub}"]
    for name, tv in truth.items():
        sv = sub.get(name)
        if sv is None:
            for k in tv: errors.append(f"{name}.{k}: MISSING"); total+=1
            continue
        for k, tvv in tv.items():
            if k in ("phase_idx","branch_idx"):  # 额外字段，规则表 10 维不含
                continue
            total+=1
            if k=="band_min": svv=sv.get("band",[None,None])[0] if isinstance(sv.get("band"),list) else sv.get("band_min")
            elif k=="band_max": svv=sv.get("band",[None,None])[1] if isinstance(sv.get("band"),list) else sv.get("band_max")
            else: svv=sv.get(k)
            if svv==tvv: correct+=1
            else: errors.append(f"{name}.{k}: {svv} != {tvv}")
    return correct, total, errors

def main():
    q1t = load_q1_truth()
    q2t = load_q2_truth()["system_B"]
    result = {}
    for p in ["P01","P02","P03","P04","P05","P06"]:
        s1 = dump_submission(p, "q01-long-context-protocol", "q01")
        s2 = dump_submission(p, "q02-protocol-adaptation", "q02")
        c1,t1,e1 = compare_q1(s1, q1t)
        c2,t2,e2 = compare_q2(s2, q2t)
        result[p] = {
            "q1_correct":c1,"q1_total":t1,"q1_rate":round(c1/t1,4) if t1 else 0,"q1_errors":e1[:30],
            "q2_correct":c2,"q2_total":t2,"q2_rate":round(c2/t2,4) if t2 else 0,"q2_errors":e2[:30],
        }
    json.dump(result, open(f"{ROOT}/scoring/machine-grade.json","w"), ensure_ascii=False, indent=2)
    # 摘要打印
    print(f"{'P':4} {'Q1':>12} {'Q2':>12}")
    for p,r in result.items():
        print(f"{p:4} {r['q1_correct']}/{r['q1_total']} ({r['q1_rate']*100:.1f}%)   {r['q2_correct']}/{r['q2_total']} ({r['q2_rate']*100:.1f}%)")

if __name__ == "__main__":
    main()
