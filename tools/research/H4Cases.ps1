# Shared inventory for portable H4 collection and independent review.
function Get-H4CaseNames([bool]$PowerShell7) {
    $names=@('desktop-boundary','cooked-empty','cooked-partial','cooked-repeated','raw-input','input-burst',
        'no-pending-read','completed-before-cancel','default-handler','mode-transitions','race-enter-biased',
        'race-concurrent','race-cancel-biased','race-outcomes','close-rpc','close-input','agent-death',
        'caller-death','caller-and-agent-death','enclosing-job','neighbor-isolation','recreate-resources',
        'impersonation-denied','helper-missing','helper-wrong')
    $shells=@('cmd','powershell')
    if($PowerShell7) { $shells+=@('pwsh-native','pwsh-readline') }
    foreach($shell in $shells) {
        $names+=@(($shell+'-empty'),($shell+'-partial'),('child-'+$shell+'-event'),('child-'+$shell+'-cooked'))
    }
    return $names
}

function Get-H4QualificationNames {
    return @('inactive-desktop','concurrent-resources','job-explicit-breakaway-allowed','job-silent-breakaway',
        'timeout-before-dispatch','timeout-during-callback','agent-death-during-callback')
}
