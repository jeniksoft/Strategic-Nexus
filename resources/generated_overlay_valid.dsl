campaign "campaign_001" {
  empire "empire_001" {
    rule "border_war_defense" {
      ministry = military_ministry
      when campaign_marker = campaign_001
      when known.save_fingerprint = h_abcdef123456
      when known.save_date = d_2200_03_01
      when known.rule_scope = entry_scope_alpha
      prefer military_posture defensive intensity 0.7
      prefer doctrine_inertia high intensity 0.4
      duration = next_session
      confidence = 0.72
      rationale = "Bounded entry-point gating should survive compile."
    }
  }
}
