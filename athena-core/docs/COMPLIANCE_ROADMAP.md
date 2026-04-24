# ATHENA — Compliance & Certification Roadmap

**Version:** 1.1.2  
**Status:** Planning  
**Classification:** Internal Reference

---

## 1. Compliance Philosophy

### 1.1 Core Principle

> *"Certification is a contractual path, not an architectural limitation."*

ATHENA is designed **certifiable from inception**, not retrofitted for compliance. The architecture embeds security and auditability as first-class requirements, enabling certification when contractually required.

### 1.2 Positioning

| Aspect | Position |
|--------|----------|
| Default state | Certifiable, not certified |
| Certification trigger | Contract requirement |
| Cost bearer | Included in contract pricing |
| Timeline | Defined per engagement |

---

## 2. Target Standards Overview

| Standard | Scope | Priority | Status |
|----------|-------|----------|--------|
| **ISO/IEC 27001** | Information Security Management | High | Baseline target |
| **Common Criteria (EAL4+)** | Product Security Evaluation | High | Primary certification |
| **NIST SP 800-53** | Security Controls (US Federal) | High | Control mapping |
| **CMMC Level 2** | DoD Contractor Cybersecurity | Medium | Conditional |
| **MIL-STD-498** | Software Development (Military) | Low | Contract-specific |
| **DO-178C** | Airborne Software | Low | Out of scope (non-embedded) |
| **NATO STANAG** | Alliance Interoperability | Low | Contract-specific |

---

## 3. ISO/IEC 27001 — Information Security Management

### 3.1 Overview

ISO 27001 establishes requirements for an Information Security Management System (ISMS). For ATHENA, this applies to:

- Development environment
- Deployment procedures
- Support operations
- Data handling

### 3.2 Relevance to ATHENA

| Aspect | Application |
|--------|-------------|
| Scope | Vendor operations + product security features |
| Certification entity | Vendor organization |
| Product impact | Demonstrates organizational security maturity |

### 3.3 Control Domains (Annex A)

| Domain | ATHENA Alignment |
|--------|------------------|
| A.5 Information Security Policies | Documented in THREAT_MODEL.md |
| A.6 Organization of Information Security | Defined in operations |
| A.7 Human Resource Security | Vendor HR policies |
| A.8 Asset Management | Data classification procedures |
| A.9 Access Control | Role-based access, least privilege |
| A.10 Cryptography | At-rest encryption, code signing |
| A.11 Physical Security | Client responsibility (documented) |
| A.12 Operations Security | Logging, monitoring, change management |
| A.13 Communications Security | Air-gap default, network isolation |
| A.14 System Acquisition/Development | Secure SDLC, V&V procedures |
| A.15 Supplier Relationships | SBOM, supply chain controls |
| A.16 Incident Management | Procedures defined |
| A.17 Business Continuity | Backup, recovery procedures |
| A.18 Compliance | This roadmap |

### 3.4 Timeline & Artifacts

| Phase | Duration | Artifacts |
|-------|----------|-----------|
| Gap analysis | 2 months | Gap report, remediation plan |
| ISMS implementation | 4 months | Policies, procedures, records |
| Internal audit | 1 month | Audit report, corrective actions |
| Certification audit (Stage 1) | 1 month | Documentation review |
| Certification audit (Stage 2) | 1 month | Implementation verification |
| **Total** | **9-12 months** | ISO 27001 certificate |

### 3.5 Dependencies

- Organizational commitment
- Dedicated ISMS owner
- Budget for certification body

---

## 4. Common Criteria (ISO/IEC 15408) — EAL4+

### 4.1 Overview

Common Criteria provides a framework for security evaluation of IT products. EAL4 (Evaluation Assurance Level 4) represents "methodically designed, tested, and reviewed" — the highest level commonly achieved for commercial software.

### 4.2 Target: EAL4+ (Augmented)

| Aspect | Specification |
|--------|---------------|
| Base level | EAL4 |
| Augmentation | ALC_FLR.2 (Flaw Reporting) |
| Rationale | Practical ceiling for commercial software; higher levels require formal methods |

### 4.3 Protection Profile Alignment

ATHENA should align with relevant Protection Profiles (PPs) where available:

| Candidate PP | Relevance |
|--------------|-----------|
| General Purpose Operating System PP | Limited (ATHENA is application) |
| Application Software PP | Moderate fit |
| No specific DSS PP exists | Custom Security Target required |

### 4.4 Security Target (ST) Components

| Component | ATHENA Mapping |
|-----------|----------------|
| **TOE Description** | ATHENA system boundary definition |
| **Security Problem Definition** | THREAT_MODEL.md threats |
| **Security Objectives** | Derived from threats |
| **Security Functional Requirements (SFRs)** | See §4.5 |
| **Security Assurance Requirements (SARs)** | EAL4 package |

### 4.5 Anticipated Security Functional Requirements

| Class | SFR | ATHENA Implementation |
|-------|-----|----------------------|
| FAU (Audit) | FAU_GEN.1 | Immutable logging |
| FAU (Audit) | FAU_SAR.1 | Audit review capability |
| FCS (Crypto) | FCS_COP.1 | Cryptographic operations (hashing, signing) |
| FDP (Data Protection) | FDP_ACC.1 | Access control policy |
| FDP (Data Protection) | FDP_IFC.1 | Information flow control |
| FIA (Auth) | FIA_UAU.1 | User authentication |
| FIA (Auth) | FIA_UID.1 | User identification |
| FMT (Management) | FMT_MSA.1 | Security attribute management |
| FMT (Management) | FMT_SMR.1 | Security roles |
| FPT (Protection) | FPT_TST.1 | Self-test (integrity verification) |

### 4.6 Assurance Requirements (EAL4)

| Class | Component | Deliverable |
|-------|-----------|-------------|
| ADV (Development) | ADV_ARC.1 | Security architecture description |
| ADV (Development) | ADV_FSP.4 | Complete functional specification |
| ADV (Development) | ADV_IMP.1 | Implementation representation |
| ADV (Development) | ADV_TDS.3 | Basic modular design |
| AGD (Guidance) | AGD_OPE.1 | Operational user guidance |
| AGD (Guidance) | AGD_PRE.1 | Preparative procedures |
| ALC (Lifecycle) | ALC_CMC.4 | Production support procedures |
| ALC (Lifecycle) | ALC_CMS.4 | Problem tracking CM coverage |
| ALC (Lifecycle) | ALC_DEL.1 | Delivery procedures |
| ALC (Lifecycle) | ALC_DVS.1 | Development security |
| ALC (Lifecycle) | ALC_LCD.1 | Developer defined lifecycle |
| ALC (Lifecycle) | ALC_TAT.1 | Well-defined development tools |
| ALC (Lifecycle) | ALC_FLR.2 | Flaw reporting (augmentation) |
| ATE (Tests) | ATE_COV.2 | Analysis of coverage |
| ATE (Tests) | ATE_DPT.2 | Testing: security enforcing modules |
| ATE (Tests) | ATE_FUN.1 | Functional testing |
| ATE (Tests) | ATE_IND.2 | Independent testing |
| AVA (Vulnerability) | AVA_VAN.3 | Focused vulnerability analysis |

### 4.7 Timeline & Artifacts

| Phase | Duration | Artifacts |
|-------|----------|-----------|
| Security Target development | 3 months | ST document |
| Evidence preparation | 6 months | ADV, AGD, ALC, ATE documents |
| Lab selection & contracting | 1 month | Evaluation contract |
| Evaluation (ITSEF) | 6-9 months | Evaluation Technical Report |
| Certification decision | 1-2 months | Certificate, Certification Report |
| **Total** | **18-24 months** | CC Certificate (EAL4+) |

### 4.8 Dependencies

- Security Target definition (requires architectural freeze)
- Evaluation lab selection (ITSEF in target market)
- Certification body (scheme: US NIAP, DE BSI, etc.)
- Budget: ~$300K-$500K+ depending on scope

### 4.9 Scheme Considerations

| Scheme | Market | Notes |
|--------|--------|-------|
| NIAP (US) | US Government | CCRA member, mutual recognition |
| BSI (Germany) | EU, NATO | CCRA member, strong defense reputation |
| ANSSI (France) | EU, NATO | CCRA member |
| CCCS (Canada) | Canada, Five Eyes | CCRA member |

**Recommendation:** Select scheme based on primary market. NIAP for US focus, BSI for EU/NATO focus.

---

## 5. NIST SP 800-53 — Security Control Mapping

### 5.1 Overview

NIST SP 800-53 defines security controls for US federal information systems. Mapping ATHENA controls to 800-53 facilitates:

- FedRAMP readiness (if cloud version future)
- DoD RMF compliance
- Federal procurement

### 5.2 Control Family Mapping

| Family | ID | ATHENA Control | Reference |
|--------|-----|----------------|-----------|
| **Access Control** | AC-2 | Account management | THREAT_MODEL S-01, S-02 |
| | AC-3 | Access enforcement | Role-based access |
| | AC-6 | Least privilege | Architecture principle |
| | AC-17 | Remote access | N/A (air-gapped default) |
| **Audit** | AU-2 | Audit events | Immutable logging |
| | AU-3 | Audit content | Manifests, timestamps |
| | AU-9 | Audit protection | Chained hashing |
| | AU-10 | Non-repudiation | Cryptographic timestamps |
| **Config Management** | CM-2 | Baseline configuration | Version control |
| | CM-3 | Change control | V&V release gate |
| | CM-6 | Configuration settings | Documented |
| | CM-7 | Least functionality | Minimal attack surface |
| **Identification/Auth** | IA-2 | User identification | Mandatory authentication |
| | IA-5 | Authenticator management | Credential policies |
| **Incident Response** | IR-4 | Incident handling | Procedures defined |
| | IR-6 | Incident reporting | Flaw reporting (ALC_FLR.2) |
| **Maintenance** | MA-2 | Controlled maintenance | Update procedures |
| | MA-3 | Maintenance tools | Signed update packages |
| **Media Protection** | MP-2 | Media access | Whitelist, scanning |
| | MP-6 | Media sanitization | Procedures defined |
| **System Protection** | SC-7 | Boundary protection | Air-gap, network isolation |
| | SC-8 | Transmission integrity | N/A (offline) |
| | SC-13 | Cryptographic protection | Hashing, signing |
| | SC-28 | Protection at rest | Encryption |
| **System Integrity** | SI-2 | Flaw remediation | Patch procedures |
| | SI-3 | Malware protection | Scanning procedures |
| | SI-7 | Software integrity | Self-hash, code signing |
| **Supply Chain** | SR-3 | Supply chain controls | SBOM, reproducible builds |
| | SR-4 | Provenance | Dependency tracking |

### 5.3 Deliverable

Full control mapping document with:
- Control ID
- Control name
- ATHENA implementation
- Evidence location
- Responsibility (vendor/integrator/client)

---

## 6. CMMC Level 2 — DoD Contractor Cybersecurity

### 6.1 Overview

Cybersecurity Maturity Model Certification (CMMC) is required for DoD contractors handling Controlled Unclassified Information (CUI). Level 2 aligns with NIST SP 800-171.

### 6.2 Applicability

| Scenario | CMMC Requirement |
|----------|------------------|
| Direct DoD contract | Required |
| Subcontract to prime | Required |
| Commercial sale, no CUI | Not required |
| Foreign military (non-US) | Not required |

### 6.3 CMMC Level 2 Domains

| Domain | Practices | ATHENA Alignment |
|--------|-----------|------------------|
| Access Control (AC) | 22 | Role-based access, least privilege |
| Audit & Accountability (AU) | 9 | Immutable logging, timestamps |
| Awareness & Training (AT) | 3 | Training plan (pending) |
| Configuration Management (CM) | 9 | Version control, baselines |
| Identification & Authentication (IA) | 11 | Mandatory auth, individual accounts |
| Incident Response (IR) | 3 | Procedures defined |
| Maintenance (MA) | 6 | Update procedures |
| Media Protection (MP) | 9 | Scanning, whitelist, sanitization |
| Personnel Security (PS) | 2 | Vendor HR policies |
| Physical Protection (PE) | 6 | Client responsibility |
| Risk Assessment (RA) | 3 | THREAT_MODEL.md |
| Security Assessment (CA) | 4 | V&V, audits |
| System & Comm Protection (SC) | 16 | Encryption, air-gap |
| System & Info Integrity (SI) | 7 | Integrity verification |

### 6.4 Certification Path

| Phase | Duration | Notes |
|-------|----------|-------|
| Self-assessment | 2 months | Gap identification |
| Remediation | 3-6 months | Close gaps |
| C3PAO assessment | 1-2 months | Third-party audit |
| Certification | 1 month | CMMC certificate |
| **Total** | **7-11 months** | |

### 6.5 Dependencies

- Applies to **vendor organization**, not just product
- Requires C3PAO (Certified Third-Party Assessment Organization)
- Budget: ~$50K-$150K depending on organization size

### 6.6 Conditional Roadmap

CMMC certification is pursued **only when**:
1. DoD contract is in negotiation, AND
2. Contract requires CMMC Level 2, AND
3. Timeline permits certification before contract start

---

## 7. Contract-Specific Standards

### 7.1 MIL-STD-498 (Software Development)

| Aspect | Position |
|--------|----------|
| Scope | Military software development process |
| Applicability | US DoD contracts requiring MIL-STD-498 |
| ATHENA position | Out of initial scope; adaptable if required |
| Effort | Process documentation alignment |

### 7.2 DO-178C (Airborne Software)

| Aspect | Position |
|--------|----------|
| Scope | Safety-critical airborne systems |
| Applicability | Embedded/airborne deployments only |
| ATHENA position | **Explicitly out of scope** (ATHENA is not embedded) |
| If required | Separate product variant with formal methods |

### 7.3 NATO STANAG

| Aspect | Position |
|--------|----------|
| Scope | NATO interoperability standards |
| Applicability | NATO member contracts |
| ATHENA position | Implemented if contractually required |
| Relevant STANAGs | To be determined per contract |

---

## 8. Dependency Matrix

```
                    ┌─────────────────┐
                    │  ISO 27001      │
                    │  (Org baseline) │
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
      ┌───────────┐  ┌───────────┐  ┌───────────┐
      │ CC EAL4+  │  │ NIST      │  │ CMMC L2   │
      │ (Product) │  │ 800-53    │  │ (Org+Prod)│
      └───────────┘  │ (Mapping) │  └───────────┘
                     └───────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
      ┌───────────┐  ┌───────────┐  ┌───────────┐
      │ MIL-STD   │  │ DO-178C   │  │ STANAG    │
      │ (Contract)│  │ (N/A)     │  │ (Contract)│
      └───────────┘  └───────────┘  └───────────┘
```

### Reading the Matrix

1. **ISO 27001** is foundational — establishes organizational security baseline
2. **CC, NIST, CMMC** can proceed in parallel after ISO 27001
3. **MIL-STD, STANAG** are contract-triggered
4. **DO-178C** is not applicable to current product scope

---

## 9. Artifact Generation Schedule

### 9.1 Core Artifacts (Always Required)

| Artifact | Purpose | Status |
|----------|---------|--------|
| Security Target (ST) | CC evaluation | Pending |
| Threat Model | CC, CMMC, 800-53 | ✅ Done |
| V&V Strategy | CC (ATE), internal QA | ✅ Done |
| SBOM | Supply chain, CC (ALC) | Architecture ready |
| User Guidance | CC (AGD), operations | Pending |
| Administrator Guidance | CC (AGD), operations | Pending |
| Architecture Description | CC (ADV_ARC) | Partially in docs |
| Functional Specification | CC (ADV_FSP) | Pending |
| Design Documentation | CC (ADV_TDS) | Pending |
| Test Documentation | CC (ATE), V&V | Pending |
| Lifecycle Documentation | CC (ALC) | Pending |
| Flaw Reporting Procedures | CC (ALC_FLR), CMMC | Pending |

### 9.2 Generation Timeline

| Quarter | Artifacts |
|---------|-----------|
| Q1 | Security Target draft, Architecture Description |
| Q2 | Functional Specification, Design Documentation |
| Q3 | Test Documentation, User/Admin Guidance |
| Q4 | Lifecycle Documentation, Flaw Reporting |
| Q5+ | Evaluation support, remediation |

---

## 10. Budget Estimates

### 10.1 Certification Costs (Approximate)

| Certification | Cost Range | Notes |
|---------------|------------|-------|
| ISO 27001 | $30K-$80K | Depends on org size, existing maturity |
| CC EAL4+ | $300K-$500K+ | Lab fees, consulting, evaluation time |
| CMMC Level 2 | $50K-$150K | C3PAO assessment, remediation |
| NIST 800-53 Mapping | $20K-$50K | Documentation effort |

### 10.2 Internal Effort

| Activity | Effort (Person-Months) |
|----------|------------------------|
| Security Target development | 3-4 PM |
| CC evidence preparation | 6-9 PM |
| Evaluation support | 3-6 PM |
| CMMC preparation | 2-4 PM |
| Ongoing maintenance | 1-2 PM/year |

---

## 11. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Evaluation delays | Medium | Schedule slip | Buffer in timeline, early lab engagement |
| Scope creep in ST | Medium | Cost increase | Freeze architecture before ST |
| Evaluator findings | High | Rework | Invest in evidence quality upfront |
| Scheme selection wrong | Low | Market mismatch | Research primary market first |
| Budget overrun | Medium | Financial | Fixed-price evaluation contracts where possible |
| CMMC requirement change | Medium | Rework | Monitor CMMC updates |

---

## 12. Governance

### 12.1 Roles

| Role | Responsibility |
|------|----------------|
| Compliance Lead | Roadmap execution, artifact coordination |
| Security Architect | Technical evidence, ST development |
| QA Lead | V&V alignment, test documentation |
| Legal/Commercial | Contract review, scheme selection |
| Executive Sponsor | Budget, strategic decisions |

### 12.2 Review Cadence

| Review | Frequency | Participants |
|--------|-----------|--------------|
| Compliance status | Monthly | Compliance Lead, Security Architect |
| Roadmap review | Quarterly | All stakeholders |
| Certification milestone | Per milestone | All + evaluator |

---

## 13. Summary Roadmap

### Year 1

| Q1 | Q2 | Q3 | Q4 |
|----|----|----|-----|
| ISO 27001 gap analysis | ISMS implementation | Internal audit | Certification audit |
| CC: ST draft | CC: Evidence prep | CC: Evidence prep | CC: Lab selection |
| NIST 800-53 mapping | — | — | — |

### Year 2

| Q1 | Q2 | Q3 | Q4 |
|----|----|----|-----|
| CC: Evaluation start | CC: Evaluation | CC: Evaluation | CC: Certification |
| CMMC: If triggered | CMMC: Assessment | CMMC: Certification | — |

### Year 3+

| Activity |
|----------|
| Maintenance, re-certification |
| Contract-specific certifications (MIL-STD, STANAG) |
| New market certifications as needed |

---

## 14. Key Messages for Stakeholders

### For Sales/BD

> *"ATHENA is designed certifiable from day one. We have a clear roadmap to ISO 27001, Common Criteria EAL4+, and CMMC Level 2. Certification timeline and scope are defined per contract."*

### For Technical Evaluators

> *"Full NIST 800-53 control mapping available. Threat model follows STRIDE. V&V strategy supports CC ATE requirements. SBOM and reproducible builds address supply chain."*

### For Procurement

> *"Certification is a contractual path. We provide all evidence artifacts. Evaluation lab and scheme selected based on your requirements."*

---

**Document End**
