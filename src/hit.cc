/*
Part of Scallop Transcript Assembler
(c) 2017 by  Mingfu Shao, Carl Kingsford, and Carnegie Mellon University.
See LICENSE for licensing.
*/

#include <cstring>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <cmath>

#include "hit.h"
#include "config.h"

hit::hit(int32_t p, const config &c)
//	: cfg(c)
{
	cfg = &c;
  bam1_core_t::pos = p;
	strand = '.';
	xs = '.';
	hi = -1;
	nh = -1;
	nm = 0;
	qlen = 0;
}

hit::hit(const hit &h )
	:bam1_core_t(h)//, cfg(h.cfg)
{
  cfg = h.cfg;
	rpos = h.rpos;
	qlen = h.qlen;
	qname = h.qname;
	strand = h.strand;
	spos = h.spos;
	xs = h.xs;
	hi = h.hi;
	nm = h.nm;
	memcpy(cigar, h.cigar, sizeof cigar);
}

/*hit& hit::operator=(const hit &h) const{
  swap(h);
  return *this;
}*/

hit::hit(bam1_t *b, const config &c)
	:bam1_core_t(b->core)//, cfg(c)
{
   cfg = &c;
	// fetch query name
	char buf[1024];
	char *q = bam_get_qname(b);
	int l = strlen(q);
	memcpy(buf, q, l);
	buf[l] = '\0';
	qname = string(buf);

	// compute rpos
	rpos = pos + (int32_t)bam_cigar2rlen(n_cigar, bam_get_cigar(b));
	qlen = (int32_t)bam_cigar2qlen(n_cigar, bam_get_cigar(b));

	// copy cigar
	assert(n_cigar <= MAX_NUM_CIGAR);
	assert(n_cigar >= 1);
	memcpy(cigar, bam_get_cigar(b), 4 * n_cigar);
}

int hit::set_tags(bam1_t *b)
{
	xs = '.';
	uint8_t *p1 = bam_aux_get(b, "XS");
	if(p1 && (*p1) == 'A') xs = bam_aux2A(p1);

	hi = -1;
	uint8_t *p2 = bam_aux_get(b, "HI");
	if(p2 && (*p2) == 'C') hi = bam_aux2i(p2);

	nh = -1;
	uint8_t *p3 = bam_aux_get(b, "NH");
	if(p3 && (*p3) == 'C') nh = bam_aux2i(p3);

	nm = 0;
	uint8_t *p4 = bam_aux_get(b, "nM");
	if(p4 && (*p4) == 'C') nm = bam_aux2i(p4);

	uint8_t *p5 = bam_aux_get(b, "NM");
	if(p5 && (*p5) == 'C') nm = bam_aux2i(p5);

	return 0;
}

int hit::set_concordance()
{
	bool concordant = false;
	if((flag & 0x10) <= 0 && (flag & 0x20) >= 1 && (flag & 0x40) >= 1 && (flag & 0x80) <= 0) concordant = true;		// F1R2
	if((flag & 0x10) >= 1 && (flag & 0x20) <= 0 && (flag & 0x40) >= 1 && (flag & 0x80) <= 0) concordant = true;		// R1F2
	if((flag & 0x10) <= 0 && (flag & 0x20) >= 1 && (flag & 0x40) <= 0 && (flag & 0x80) >= 1) concordant = true;		// F2R1
	if((flag & 0x10) >= 1 && (flag & 0x20) <= 0 && (flag & 0x40) <= 0 && (flag & 0x80) >= 1) concordant = true;		// R2F1
	return 0;
}

int hit::set_strand()
{
	strand = '.';

	//cerr << "cfg->library_type: " << cfg->library_type << endl;
	if(cfg->library_type == FR_FIRST && ((flag & 0x1) >= 1))
	{
		if((flag & 0x10) <= 0 && (flag & 0x40) >= 1 && (flag & 0x80) <= 0) strand = '-';
		if((flag & 0x10) >= 1 && (flag & 0x40) >= 1 && (flag & 0x80) <= 0) strand = '+';
		if((flag & 0x10) <= 0 && (flag & 0x40) <= 0 && (flag & 0x80) >= 1) strand = '+';
		if((flag & 0x10) >= 1 && (flag & 0x40) <= 0 && (flag & 0x80) >= 1) strand = '-';
	}

	if(cfg->library_type == FR_SECOND && ((flag & 0x1) >= 1))
	{
		if((flag & 0x10) <= 0 && (flag & 0x40) >= 1 && (flag & 0x80) <= 0) strand = '+';
		if((flag & 0x10) >= 1 && (flag & 0x40) >= 1 && (flag & 0x80) <= 0) strand = '-';
		if((flag & 0x10) <= 0 && (flag & 0x40) <= 0 && (flag & 0x80) >= 1) strand = '-';
		if((flag & 0x10) >= 1 && (flag & 0x40) <= 0 && (flag & 0x80) >= 1) strand = '+';
	}

	if(cfg->library_type == FR_FIRST && ((flag & 0x1) <= 0))
	{
		if((flag & 0x10) <= 0) strand = '-';
		if((flag & 0x10) >= 1) strand = '+';
	}

	if(cfg->library_type == FR_SECOND && ((flag & 0x1) <= 0))
	{
		if((flag & 0x10) <= 0) strand = '+';
		if((flag & 0x10) >= 1) strand = '-';
	}

	return 0;
}

int hit::build_splice_positions()
{
	spos.clear();
	int32_t p = pos;
	int32_t q = 0;
	//uint8_t *seq = bam_get_seq(b);
    for(int k = 0; k < n_cigar; k++)
	{
		if (bam_cigar_type(bam_cigar_op(cigar[k]))&2)
			p += bam_cigar_oplen(cigar[k]);

		if (bam_cigar_type(bam_cigar_op(cigar[k]))&1)
			q += bam_cigar_oplen(cigar[k]);

		if(k == 0 || k == n_cigar - 1) continue;
		if(bam_cigar_op(cigar[k]) != BAM_CREF_SKIP) continue;
		if(bam_cigar_op(cigar[k-1]) != BAM_CMATCH) continue;
		if(bam_cigar_op(cigar[k+1]) != BAM_CMATCH) continue;
		//cerr << "cfg->min_flank_length: "  << cfg->min_flank_length << endl;
		if(bam_cigar_oplen(cigar[k-1]) < cfg->min_flank_length) continue;
		if(bam_cigar_oplen(cigar[k+1]) < cfg->min_flank_length) continue;

		int32_t s = p - bam_cigar_oplen(cigar[k]);
		spos.push_back(pack(s, p));
	}
	return 0;
}

bool hit::verify_junctions()
{
	int32_t p = pos;
	int32_t q = 0;
    for(int k = 0; k < n_cigar; k++)
	{
		if (bam_cigar_type(bam_cigar_op(cigar[k]))&2)
			p += bam_cigar_oplen(cigar[k]);

		if (bam_cigar_type(bam_cigar_op(cigar[k]))&1)
			q += bam_cigar_oplen(cigar[k]);

		if(k == 0 || k == n_cigar - 1) continue;
		if(bam_cigar_op(cigar[k]) != BAM_CREF_SKIP) continue;
		if(bam_cigar_op(cigar[k-1]) != BAM_CMATCH) continue;
		if(bam_cigar_op(cigar[k+1]) != BAM_CMATCH) continue;

		int s = bam_cigar_oplen(cigar[k]);
		int m1 = bam_cigar_oplen(cigar[k-1]);
		int m2 = bam_cigar_oplen(cigar[k+1]);
		int m = (m1 < m2) ? m1 : m2;

		//if(log2(s) > log2(10) + (2 * m) && nh >= 2)
		if(log2(s) > log2(10) + (2 * m))
		{
			if(cfg->verbose >= 2) printf("detect super long junction %d with matches (%d, %d)\n", s, m1, m2);
			return false;
		}
	}
	return true;
}

bool hit::operator<(const hit &h) const
{
	if(qname < h.qname) return true;
	if(qname > h.qname) return false;
	if(hi != -1 && h.hi != -1 && hi < h.hi) return true;
	if(hi != -1 && h.hi != -1 && hi > h.hi) return false;
	return (pos < h.pos);
}

int hit::print() const
{
	// get cigar string
	ostringstream sstr;
	for(int i = 0; i < n_cigar; i++)
	{
		sstr << bam_cigar_opchr(cigar[i]) << bam_cigar_oplen(cigar[i]);
		//printf("cigar %d: op = %c, length = %3d\n", i, bam_cigar_opchr(cigar[i]), bam_cigar_oplen(cigar[i]));
	}

	// print basic information
	printf("Hit %s: [%d-%d), mpos = %d, cigar = %s, flag = %d, quality = %d, strand = %c, isize = %d, qlen = %d, hi = %d\n",
			qname.c_str(), pos, rpos, mpos, sstr.str().c_str(), flag, qual, strand, isize, qlen, hi);

	return 0;
}

int hit::get_mid_intervals(vector<int64_t> &vm, vector<int64_t> &vi, vector<int64_t> &vd) const
{
	vm.clear();
	vi.clear();
	vd.clear();
	int32_t p = pos;
    for(int k = 0; k < n_cigar; k++)
	{
		if (bam_cigar_type(bam_cigar_op(cigar[k]))&2)
		{
			p += bam_cigar_oplen(cigar[k]);
		}

		if(bam_cigar_op(cigar[k]) == BAM_CMATCH)
		{
			int32_t s = p - bam_cigar_oplen(cigar[k]);
			vm.push_back(pack(s, p));
		}

		if(bam_cigar_op(cigar[k]) == BAM_CINS)
		{
			vi.push_back(pack(p - 1, p + 1));
		}

		if(bam_cigar_op(cigar[k]) == BAM_CDEL)
		{
			int32_t s = p - bam_cigar_oplen(cigar[k]);
			vd.push_back(pack(s, p));
		}
	}
    return 0;
}

int hit::get_matched_intervals(vector<int64_t> &v) const
{
	vector<int64_t> vi, vd;
	return get_mid_intervals(v, vi, vd);
}

/*
inline bool hit_compare_by_name(const hit &x, const hit &y)
{
	if(x.qname < y.qname) return true;
	if(x.qname > y.qname) return false;
	return (x.pos < y.pos);
}

inline bool hit_compare_left(const hit &x, const hit &y)
{
	if(x.pos < y.pos) return true;
	else return false;
}

inline bool hit_compare_right(const hit &x, const hit &y)
{
	if(x.rpos < y.rpos) return true;
	return false;
}
*/

bool hit::maps_to_transcript(const transcript &t){
	//if(t.seqname != seqname ) return 0;
  //cerr << "t.seqname.c_str(): " << t.seqname << "\ttid: " << seqname << endl;
  assert(t.exons.size()>0);
  assert(pos < rpos);
  if(t.exons[0].first > pos || t.exons[t.exons.size()-1].second < rpos) return 0;


  bool started = false;
	int cur_exon = 0;

  int tolerance = 1;

  if(spos.size() == 0){
		while(cur_exon < t.exons.size() && t.exons[cur_exon].second < pos){
			cur_exon++;
    }
    if(cur_exon == t.exons.size()) return false;
		return (t.exons[cur_exon].first <= pos && t.exons[cur_exon].second >= rpos);
		//return ((t.exons[cur_exon].first <= pos + tolerance) && (t.exons[cur_exon].second >= rpos - tolerance));
	}

	int32_t start = pos;
	int32_t end = high32(spos[0]);
	while(cur_exon < t.exons.size() && t.exons[cur_exon].second < start)
		cur_exon++;
	if(cur_exon == t.exons.size())
		return false;
	//if(t.exons[cur_exon].second < end - tolerance || t.exons[cur_exon].second > end + tolerance)
	if(t.exons[cur_exon].second != end)
		return false;
	if(cur_exon + spos.size() > t.exons.size())
		return false;

	cur_exon++;
	for(int i=0; i<spos.size()-1; i++, cur_exon++){
		start = low32(spos[i]);
		end = high32(spos[i+1]);
		//if((t.exons[cur_exon].first < start - tolerance || t.exons[cur_exon].first > start + tolerance) ||
    //    (t.exons[cur_exon].second < end - tolerance || t.exons[cur_exon].second > end + tolerance))
		if(t.exons[cur_exon].first != start || t.exons[cur_exon].second != end)
      return false;
	}
	start = low32(spos[spos.size()-1]);
	end = rpos;
	if(cur_exon == t.exons.size())
    return false;
	//if((t.exons[cur_exon].first < start - tolerance || t.exons[cur_exon].first > start + tolerance) ||
  //            (t.exons[cur_exon].second < end - tolerance || t.exons[cur_exon].second > end + tolerance))
	if(t.exons[cur_exon].first != start || t.exons[cur_exon].second < end )
    return false;

	return true;
}
