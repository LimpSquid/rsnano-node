$ErrorActionPreference = "Continue"

if ( ${env:BETA} -eq 1 ) {
    $network_cfg = "beta"
}
elseif ( ${env:TEST} -eq 1 ) {
    $network_cfg = "test"
}
else {
    $network_cfg = "live"
}

if ( ${env:GITHUB_REPOSITORY} -eq "simpago/rsnano-node" ) {
    $directory=$network_cfg
}
else {
    $directory=${env:S3_BUILD_DIRECTORY}+"/"+$network_cfg
}

$exe = Resolve-Path -Path $env:GITHUB_WORKSPACE\build\nano-node-*-win64.exe
$zip = Resolve-Path -Path $env:GITHUB_WORKSPACE\build\nano-node-*-win64.zip

((Get-FileHash $exe).hash)+" "+(split-path -Path $exe -Resolve -leaf) | Out-file -FilePath "$exe.sha256"
((Get-FileHash $zip).hash)+" "+(split-path -Path $zip -Resolve -leaf) | Out-file -FilePath "$zip.sha256"

aws s3 cp $exe s3://repo.nano.org/$directory/binaries/nano-node-$env:TAG-win64.exe --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
aws s3 cp "$exe.sha256" s3://repo.nano.org/$directory/binaries/nano-node-$env:TAG-win64.exe.sha256 --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
aws s3 cp "$zip" s3://repo.nano.org/$directory/binaries/nano-node-$env:TAG-win64.zip --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers
aws s3 cp "$zip.sha256" s3://repo.nano.org/$directory/binaries/nano-node-$env:TAG-win64.zip.sha256 --grants read=uri=http://acs.amazonaws.com/groups/global/AllUsers