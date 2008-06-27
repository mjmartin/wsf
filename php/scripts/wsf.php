<?php
/**
 * Copyright (c) 2005-2008 WSO2, Inc. http://wso2.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** The version of this WSO2 WSF for PHP file */
define('WSF_VERSION', '1.3.2');

function ws_request($payload, $options = array(NULL))
{
    $payloadString = $payload;
    
    if ($payload instanceof domDocument)
    {
        $payloadString = $payload->saveXML();
    }

    try
    {
        $requestMessage = $payload;
        if (!($payload instanceof WSMessage))
        {
            $requestMessage = new WSMessage($payloadString);
        }

        $client = new WSClient($options);

        $resMessage = $client->request($requestMessage);

        return $resMessage;

    }
    catch (Exception $e)
    {
        throw $e;
    }
}

function ws_send($payload, $options = array(NULL))
{
    $payloadString = $payload;
    
    if ($payload instanceof domDocument)
    {
        $payloadString = $payload->saveXML();
    }

    try
    {
        $requestMessage = $payload;
        if (!($payload instanceof WSMessage))
        {
            $requestMessage = new WSMessage($payloadString);
        }

        $client = new WSClient($options);

        $client->send($requestMessage);

        return;

    }
    catch (Exception $e)
    {
        throw $e;
    }
}


function ws_reply($options)
{

    $svr = new WSService($options);

    $svr->reply();
}

function ws_generate_wsdl($service_name, $fn_arry, $class_arry, $binding_style, 
                          $wsdl_version, $request_uri, $op_arry,
                          $classmap = NULL, $annotations = NULL)
{
    require_once("wsdl/WS_WSDL_Creator.php");
    require_once("wsdl/WS_WSDL_Consts.php");
    require_once('dynamic_invocation/wsf_wsdl_consts.php');

    ws_log_write(__FILE__, __LINE__, WSF_LOG_DEBUG, "class arry:".print_r($class_arry, TRUE));

    if ($binding_style == WS_WSDL_Const::WSF_WSDL_RPC_ENCODED) {
        $binding_style = WS_WSDL_Const::WSF_WSDL_RPC;
    }
    else if($binding_style == NULL) {
        $binding_style = WS_WSDL_Const::WSF_WSDL_DOCLIT;
    }
    if ($binding_style != WS_WSDL_Const::WSF_WSDL_RPC && $binding_style != WS_WSDL_Const::WSF_WSDL_DOCLIT) {
        ws_log_write(__FILE__, __LINE__, WSF_LOG_ERROR, "Wrong binding style $binding_style");
        echo "Error in generating the WSDL\n";
        exit(0);
    }

    $namespace = WS_WSDL_Const::WS_WSDL_DEFAULT_NS;

    /* obtain the namespace form the first class name */
    $first_class_name = "";
    $first_op_name = "";
    if($class_arry && is_array($class_arry)) {
        foreach($class_arry as $class_name => $value) {
            $first_class_name = $class_name;
            break;
        }
    }
    else {
        foreach($op_arry as $op_name => $value) {
            $first_op_name = $op_name;
        }
    }

    if($first_class_name) {
        try {
            $class = new ReflectionClass($first_class_name);
            $class_comment = $class->getDocComment();
    
            if(preg_match_all('|@namespace\s+([^\s]+).*|', $class_comment, $matches, PREG_SET_ORDER)) {
                $namespace = $matches[0][1];
            }

        }
        catch(Exception $e) {
            //if the class doesn't exist, we just continue to use the default namespace
        }
    }
    else if($first_op_name) {
        try {
            $op_comment = new ReflectionFunction($first_op_name);
            if(preg_match_all('|@namespace\s+([^\s]+).*|', $op_comment, $matches, PREG_SET_ORDER)) {
                $namespace = $matches[0][1];
            }
        }
        catch (Exception $e) {
            //if the class doesn't exist, we just continue to use the default namespace
        }
    }

    $wsdl = new WS_WSDL_Creator($fn_arry, $class_arry, $service_name, $request_uri,
              $binding_style,$namespace, $wsdl_version, $op_arry, $classmap, $annotations);
    $wsdl_out = $wsdl->WS_WSDL_Out();


    /* converting wsdl1.1 to 2 */
    if(strcmp($wsdl_version , WS_WSDL_Const::WSF_WSDL_VERSION2_0) == 0) {
        $converted_wsdl = convert_to_wsdl20($wsdl_out);
        return $converted_wsdl;
    }
    else
    {
        return $wsdl_out;
    }
}


function convert_to_wsdl20($wsdl_out)
{
    $wsdl_dom = new DomDocument();
    $xslt_wsdl_20_dom = new DOMDocument();
    $xslt_11_to_20_dom = new DOMDocument();
    $xslt_11_to_20_dom->preserveWhiteSpace = false;
    $xslt = new XSLTProcessor();
 
    
    $xslt_str = file_get_contents(WS_WSDL_Const::WSF_WSDL_WSDL11_2_2_CONVERTORS, TRUE);
    if(!($xslt_wsdl_20_dom->loadXML($xslt_str))) {

        ws_log_write(__FILE__, __LINE__, WSF_LOG_ERROR, "Error loading the wsdl11towsdl2 convertor script");
        echo "Error in generating the WSDL\n";
        exit(0);
    }
    
    $xslt->importStyleSheet($xslt_wsdl_20_dom);
    $wsdl_dom->loadXML($wsdl_out);
    $xslt_11_to_20_dom->loadXML($xslt->transformToXML($wsdl_dom));
    $doc_ele = $xslt_11_to_20_dom->documentElement;
    foreach($doc_ele->childNodes as $child) {
        if($child->nodeType == XML_COMMENT_NODE ) {
           $old_child = $doc_ele->removeChild($child); 
            //echo "asdasd";
        }
    }
    
    return $xslt_11_to_20_dom->saveXML();
}


?>
